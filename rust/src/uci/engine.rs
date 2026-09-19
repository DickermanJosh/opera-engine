// UCI Engine Main Coordinator and Command Handler
//
// This module provides the main UCIEngine struct that coordinates all UCI protocol
// operations with thread-safe state management and async command processing.

use std::sync::Arc;
use tokio::sync::{broadcast, mpsc, oneshot};
use tokio::time::Instant;
use tracing::{debug, error, info, instrument, warn};

use crate::error::{UCIError, UCIResult};
use crate::uci::commands::{TimeControl, UCICommand};
use crate::uci::parser::ZeroCopyParser;
use crate::uci::state::{EngineConfig, EngineState, StateChangeEvent, UCIState};

/// Main UCI engine coordinator with async command processing
pub struct UCIEngine {
    /// Thread-safe state management
    state: Arc<UCIState>,

    /// Command parser for UCI protocol (mutable for statistics tracking)
    parser: parking_lot::Mutex<ZeroCopyParser>,

    /// Command processing channel
    command_tx: mpsc::UnboundedSender<EngineCommand>,
    command_rx: Option<mpsc::UnboundedReceiver<EngineCommand>>,

    /// Response channel for sending UCI responses
    response_tx: broadcast::Sender<String>,

    /// Engine identification information
    id_info: EngineIdentification,

    /// Startup timestamp
    startup_time: Instant,
    processing: tokio::sync::Mutex<()>,
    #[cfg(feature = "ffi")]
    search: tokio::sync::Mutex<super::search_session::SearchSession>,
}

/// Engine identification information for UCI protocol
#[derive(Debug, Clone)]
pub struct EngineIdentification {
    pub name: String,
    pub author: String,
    pub version: String,
}

impl Default for EngineIdentification {
    fn default() -> Self {
        Self {
            name: crate::NAME.to_string(),
            author: crate::AUTHOR.to_string(),
            version: crate::VERSION.to_string(),
        }
    }
}

/// Internal engine commands for async processing
#[derive(Debug)]
pub enum EngineCommand {
    /// Process a UCI command from input
    ProcessCommand {
        command: String,
        response_tx: oneshot::Sender<UCIResult<()>>,
    },
    /// Stop current search operation
    StopSearch {
        response_tx: oneshot::Sender<UCIResult<()>>,
    },
    /// Shutdown the engine gracefully  
    Shutdown {
        response_tx: oneshot::Sender<UCIResult<()>>,
    },
    /// Reset engine to clean state
    Reset {
        response_tx: oneshot::Sender<UCIResult<()>>,
    },
}

/// Search result information
#[derive(Debug, Clone)]
pub struct SearchResult {
    pub best_move: String,
    pub ponder_move: Option<String>,
    pub depth: u32,
    pub score: i32,
    pub nodes: u64,
    pub time_ms: u64,
    pub nps: u64,
    pub principal_variation: Vec<String>,
}

impl UCIEngine {
    /// Create a new UCI engine with default configuration
    pub fn new() -> Self {
        Self::with_config(EngineConfig::default())
    }

    /// Create a new UCI engine with custom configuration
    pub fn with_config(config: EngineConfig) -> Self {
        let state = Arc::new(UCIState::new());

        // Initialize state with provided configuration
        state
            .update_config(|cfg| *cfg = config)
            .expect("Failed to set initial config");

        let (command_tx, command_rx) = mpsc::unbounded_channel();
        let (response_tx, _) = broadcast::channel(4096);

        Self {
            state,
            parser: parking_lot::Mutex::new(ZeroCopyParser::new()),
            command_tx,
            command_rx: Some(command_rx),
            response_tx,
            id_info: EngineIdentification::default(),
            startup_time: Instant::now(),
            processing: tokio::sync::Mutex::new(()),
            #[cfg(feature = "ffi")]
            search: tokio::sync::Mutex::new(super::search_session::SearchSession::default()),
        }
    }

    /// Initialize the engine and transition to ready state
    #[instrument(skip(self))]
    pub async fn initialize(&self) -> UCIResult<()> {
        info!("Initializing UCI engine");

        // Perform initialization steps
        self.state
            .transition_to(EngineState::Ready, "Engine initialization complete")?;

        info!(
            elapsed_ms = self.startup_time.elapsed().as_millis(),
            "UCI engine initialization complete"
        );

        Ok(())
    }

    /// Start the main command processing loop
    #[instrument(skip(self))]
    pub async fn run_command_loop(&mut self) -> UCIResult<()> {
        let mut command_rx = self.command_rx.take().ok_or_else(|| UCIError::Internal {
            message: "Command receiver already taken".to_string(),
        })?;

        info!("Starting UCI engine command processing loop");

        while let Some(command) = command_rx.recv().await {
            if let Err(e) = self.handle_engine_command(command).await {
                error!(error = ?e, "Error processing engine command");
                // Don't break the loop for individual command errors
            }
            if self.state() == EngineState::Stopping {
                break;
            }
        }

        info!("UCI engine command processing loop ended");
        Ok(())
    }

    /// Handle internal engine commands
    async fn handle_engine_command(&self, command: EngineCommand) -> UCIResult<()> {
        match command {
            EngineCommand::ProcessCommand {
                command,
                response_tx,
            } => {
                let result = self.process_uci_command(&command).await;
                let _ = response_tx.send(result);
            }
            EngineCommand::StopSearch { response_tx } => {
                let result = self.stop_search().await;
                let _ = response_tx.send(result);
            }
            EngineCommand::Shutdown { response_tx } => {
                let result = self.shutdown().await;
                let _ = response_tx.send(result);
                return Err(UCIError::Internal {
                    message: "Engine shutdown requested".to_string(),
                });
            }
            EngineCommand::Reset { response_tx } => {
                let result = self.reset().await;
                let _ = response_tx.send(result);
            }
        }

        Ok(())
    }

    /// Process a UCI command string
    #[instrument(skip(self, command_str))]
    async fn process_uci_command(&self, command_str: &str) -> UCIResult<()> {
        let _command_guard = self.processing.lock().await;
        debug!(command = command_str, "Processing UCI command");

        // Parse the command (need mutable lock for statistics)
        let command = self.parser.lock().parse_command(command_str)?;

        // Dispatch to appropriate handler
        match command {
            UCICommand::Uci => self.handle_uci_command().await,
            UCICommand::Debug(enabled) => self.handle_debug_command(enabled).await,
            UCICommand::IsReady => self.handle_isready_command().await,
            UCICommand::SetOption { name, value } => {
                self.handle_setoption_command(name, value).await
            }
            UCICommand::Register { later, name, code } => {
                self.handle_register_command(later, name, code).await
            }
            UCICommand::UciNewGame => self.handle_ucinewgame_command().await,
            UCICommand::Position { position, moves } => {
                self.handle_position_command(position, moves).await
            }
            UCICommand::Go(time_control) => self.handle_go_command(time_control).await,
            UCICommand::Stop => self.handle_stop_command().await,
            UCICommand::PonderHit => self.handle_ponderhit_command().await,
            UCICommand::Quit => self.handle_quit_command().await,
        }
    }

    /// Handle UCI identification command
    async fn handle_uci_command(&self) -> UCIResult<()> {
        self.send_response(&format!("id name {}", self.id_info.name))?;
        self.send_response(&format!("id author {}", self.id_info.author))?;

        // Send available options
        self.send_uci_options()?;

        self.send_response("uciok")?;
        Ok(())
    }

    /// Handle debug mode command
    async fn handle_debug_command(&self, enabled: bool) -> UCIResult<()> {
        self.state.set_debug_mode(enabled);
        debug!(enabled, "Debug mode changed");
        Ok(())
    }

    /// Handle engine ready query
    async fn handle_isready_command(&self) -> UCIResult<()> {
        let current_state = self.state.current_state();

        if current_state.can_accept_commands() {
            self.send_response("readyok")?;
        } else {
            warn!(
                state = ?current_state,
                "Received isready when engine not in ready state"
            );

            // Still send readyok but log the warning
            self.send_response("readyok")?;
        }

        Ok(())
    }

    /// Handle set option command
    async fn handle_setoption_command(&self, name: &str, value: Option<&str>) -> UCIResult<()> {
        let bad = || UCIError::Protocol {
            message: format!("Unsupported option or invalid value: {name}"),
        };
        #[cfg(feature = "ffi")]
        {
            let mut search = self.search.lock().await;
            let normalized_name = name.split_whitespace().collect::<Vec<_>>().join(" ");
            match normalized_name.to_ascii_lowercase().as_str() {
                "hash" => {
                    let n = value
                        .and_then(|v| v.parse::<u32>().ok())
                        .filter(|n| (1..=128).contains(n))
                        .ok_or_else(bad)?;
                    search.hash_mb = n;
                    self.state.update_config(|c| c.hash_size_mb = n)?;
                }
                "threads" => {
                    if value != Some("1") {
                        return Err(bad());
                    }
                    self.state.update_config(|c| {
                        c.thread_count = 1;
                        c.multithread_enabled = false;
                    })?;
                }
                "ponder" => {
                    let on = match value {
                        Some("true") => true,
                        Some("false") => false,
                        _ => return Err(bad()),
                    };
                    self.state.update_config(|c| c.ponder_enabled = on)?;
                }
                "morphystyle" => {
                    search.morphy = match value {
                        Some("true") => true,
                        Some("false") => false,
                        _ => return Err(bad()),
                    };
                }
                "move overhead" => {
                    search.overhead_ms = value
                        .and_then(|v| v.parse::<u64>().ok())
                        .filter(|n| *n <= 5000)
                        .ok_or_else(bad)?;
                }
                "clear hash" => {
                    if value.is_some() {
                        return Err(bad());
                    }
                    search.stop().await?;
                } // Each worker owns a fresh table.
                _ => return Err(bad()),
            }
            return Ok(());
        }
        #[cfg(not(feature = "ffi"))]
        Err(bad())
    }

    /// Handle registration command (no-op for open source engine)
    async fn handle_register_command(
        &self,
        _later: bool,
        _name: Option<&str>,
        _code: Option<&str>,
    ) -> UCIResult<()> {
        // Open source engine - no registration required
        debug!("Registration command received (no-op for open source engine)");
        Ok(())
    }

    /// Handle new game command
    async fn handle_ucinewgame_command(&self) -> UCIResult<()> {
        #[cfg(feature = "ffi")]
        self.search.lock().await.new_game().await?;
        self.state.reset()
    }

    async fn handle_position_command(
        &self,
        position: crate::uci::commands::Position<'_>,
        moves: Vec<crate::uci::commands::ChessMove<'_>>,
    ) -> UCIResult<()> {
        #[cfg(feature = "ffi")]
        {
            return self.search.lock().await.position(position, moves).await;
        }
        #[cfg(not(feature = "ffi"))]
        Err(UCIError::Engine {
            message: "C++ FFI required".into(),
        })
    }

    async fn handle_go_command(&self, time_control: TimeControl) -> UCIResult<()> {
        #[cfg(feature = "ffi")]
        {
            return self
                .search
                .lock()
                .await
                .go(time_control, self.state.clone(), self.response_tx.clone())
                .await;
        }
        #[cfg(not(feature = "ffi"))]
        Err(UCIError::Engine {
            message: "C++ FFI required for search".into(),
        })
    }

    /// Handle stop command
    async fn handle_stop_command(&self) -> UCIResult<()> {
        self.stop_search().await
    }

    /// Handle ponder hit command
    async fn handle_ponderhit_command(&self) -> UCIResult<()> {
        if self.state() == EngineState::Pondering {
            self.state
                .transition_to(EngineState::Searching, "Ponder hit")?;
        }
        #[cfg(feature = "ffi")]
        self.search.lock().await.ponderhit();
        Ok(())
    }

    /// Handle quit command
    async fn handle_quit_command(&self) -> UCIResult<()> {
        info!("Quit command received");
        self.shutdown().await
    }

    /// Stop current search operation
    async fn stop_search(&self) -> UCIResult<()> {
        #[cfg(feature = "ffi")]
        self.search.lock().await.stop().await?;
        Ok(())
    }

    /// Stop and join worker before transitioning out of the ready lifecycle.
    async fn shutdown(&self) -> UCIResult<()> {
        self.stop_search().await?;
        if self.state() == EngineState::Stopping {
            return Ok(());
        }
        self.state
            .transition_to(EngineState::Stopping, "Engine shutdown requested")
    }

    /// Reset engine to clean state
    async fn reset(&self) -> UCIResult<()> {
        info!("Resetting UCI engine");

        // Stop any ongoing search
        if self.state.current_state().is_computing() {
            let _ = self.stop_search().await;
        }

        // Reset state
        self.state.reset()?;

        Ok(())
    }

    /// Send UCI options for the uci command
    fn send_uci_options(&self) -> UCIResult<()> {
        for option in [
            "option name Hash type spin default 16 min 1 max 128",
            "option name Threads type spin default 1 min 1 max 1",
            "option name Ponder type check default false",
            "option name MorphyStyle type check default false",
            "option name Move Overhead type spin default 10 min 0 max 5000",
            "option name Clear Hash type button",
        ] {
            self.send_response(option)?;
        }
        Ok(())
    }

    /// Send a response to the UCI interface
    fn send_response(&self, response: &str) -> UCIResult<()> {
        debug!(response, "Sending UCI response");

        // Send via broadcast channel (non-blocking)
        match self.response_tx.send(response.to_string()) {
            Ok(_) => Ok(()),
            Err(_) => {
                // No active receivers is OK - might happen during shutdown
                Ok(())
            }
        }
    }

    /// Get current engine state
    pub fn state(&self) -> EngineState {
        self.state.current_state()
    }

    /// Get engine statistics
    pub fn statistics(&self) -> crate::uci::state::EngineStatistics {
        self.state.statistics()
    }

    /// Subscribe to engine responses
    pub fn subscribe_responses(&self) -> broadcast::Receiver<String> {
        self.response_tx.subscribe()
    }

    /// Subscribe to state changes
    pub fn subscribe_state_changes(&self) -> broadcast::Receiver<StateChangeEvent> {
        self.state.subscribe_state_changes()
    }

    /// Get command sender for external command processing
    pub fn command_sender(&self) -> mpsc::UnboundedSender<EngineCommand> {
        self.command_tx.clone()
    }

    /// Process a single UCI command synchronously (for testing)
    pub async fn process_command(&self, command: &str) -> UCIResult<()> {
        self.process_uci_command(command).await
    }
}

impl Default for UCIEngine {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use tokio::time::Duration;

    #[tokio::test]
    async fn test_engine_initialization() {
        let engine = UCIEngine::new();

        assert_eq!(engine.state(), EngineState::Initializing);

        engine.initialize().await.unwrap();
        assert_eq!(engine.state(), EngineState::Ready);
    }

    #[tokio::test]
    async fn test_uci_command() {
        let engine = UCIEngine::new();
        engine.initialize().await.unwrap();

        let mut responses = engine.subscribe_responses();

        engine.process_command("uci").await.unwrap();

        // Should receive identification and options
        let response1 = responses.recv().await.unwrap();
        assert!(response1.starts_with("id name"));

        let response2 = responses.recv().await.unwrap();
        assert!(response2.starts_with("id author"));

        // Skip option responses
        while let Ok(response) =
            tokio::time::timeout(Duration::from_millis(10), responses.recv()).await
        {
            if response.is_ok() && response.unwrap() == "uciok" {
                break;
            }
        }
    }

    #[tokio::test]
    async fn test_debug_command() {
        let engine = UCIEngine::new();
        engine.initialize().await.unwrap();

        assert!(!engine.state.is_debug_mode());

        engine.process_command("debug on").await.unwrap();
        assert!(engine.state.is_debug_mode());

        engine.process_command("debug off").await.unwrap();
        assert!(!engine.state.is_debug_mode());
    }

    #[tokio::test]
    async fn test_isready_command() {
        let engine = UCIEngine::new();
        engine.initialize().await.unwrap();

        let mut responses = engine.subscribe_responses();

        engine.process_command("isready").await.unwrap();

        let response = responses.recv().await.unwrap();
        assert_eq!(response, "readyok");
    }

    #[tokio::test]
    async fn test_setoption_commands() {
        let engine = UCIEngine::new();
        engine.initialize().await.unwrap();

        // Test hash size option
        engine
            .process_command("setoption name Hash value 64")
            .await
            .unwrap();
        let config = engine.state.config();
        assert_eq!(config.hash_size_mb, 64);

        // Test threads option
        engine
            .process_command("setoption name Threads value 1")
            .await
            .unwrap();
        let config = engine.state.config();
        assert_eq!(config.thread_count, 1);
        assert!(!config.multithread_enabled);

        // Test ponder option
        engine
            .process_command("setoption name Ponder value true")
            .await
            .unwrap();
        let config = engine.state.config();
        assert!(config.ponder_enabled);
    }

    #[tokio::test]
    async fn test_go_command() {
        let engine = UCIEngine::new();
        engine.initialize().await.unwrap();

        let mut responses = engine.subscribe_responses();
        let mut state_changes = engine.subscribe_state_changes();

        engine.process_command("go movetime 100").await.unwrap();

        // Should transition to searching
        let state_change = tokio::time::timeout(Duration::from_millis(500), state_changes.recv())
            .await
            .unwrap()
            .unwrap();
        assert_eq!(state_change.to, EngineState::Searching);

        // Should eventually get a best move response
        let response = tokio::time::timeout(Duration::from_millis(500), async {
            loop {
                let line = responses.recv().await?;
                if line.starts_with("bestmove ") {
                    break Ok::<_, broadcast::error::RecvError>(line);
                }
            }
        })
        .await
        .unwrap()
        .unwrap();
        assert!(response.starts_with("bestmove"));

        // Should return to ready state
        let state_change = tokio::time::timeout(Duration::from_millis(500), state_changes.recv())
            .await
            .unwrap()
            .unwrap();
        assert_eq!(state_change.to, EngineState::Ready);
    }

    #[tokio::test]
    async fn test_stop_command() {
        let engine = UCIEngine::new();
        engine.initialize().await.unwrap();

        let mut state_changes = engine.subscribe_state_changes();

        // Start a search
        engine.process_command("go infinite").await.unwrap();

        // Wait for search to start
        let state_change = tokio::time::timeout(Duration::from_millis(500), state_changes.recv())
            .await
            .unwrap()
            .unwrap();
        assert_eq!(state_change.to, EngineState::Searching);

        // Stop the search
        engine.process_command("stop").await.unwrap();

        // Should return to ready
        let state_change = tokio::time::timeout(Duration::from_millis(500), state_changes.recv())
            .await
            .unwrap()
            .unwrap();
        assert_eq!(state_change.to, EngineState::Ready);
    }

    #[tokio::test]
    async fn test_ucinewgame_command() {
        let engine = UCIEngine::new();
        engine.initialize().await.unwrap();

        engine.process_command("ucinewgame").await.unwrap();

        // Should reset state
        assert_eq!(engine.state(), EngineState::Ready);
    }

    #[tokio::test]
    async fn test_quit_command() {
        let engine = UCIEngine::new();
        engine.initialize().await.unwrap();

        let mut state_changes = engine.subscribe_state_changes();

        engine.process_command("quit").await.unwrap();

        // Should transition to stopping state
        let state_change = tokio::time::timeout(Duration::from_millis(500), state_changes.recv())
            .await
            .unwrap()
            .unwrap();
        assert_eq!(state_change.to, EngineState::Stopping);
    }

    #[tokio::test]
    async fn test_command_sender_interface() {
        let engine = UCIEngine::new();
        engine.initialize().await.unwrap();

        // Test the command sender exists and can be cloned
        let command_sender = engine.command_sender();
        let command_sender_clone = command_sender.clone();

        // Test that we can send commands (but we won't wait for processing
        // since no command loop is running in this test)
        let (response_tx, _response_rx) = oneshot::channel();

        let send_result = command_sender.send(EngineCommand::ProcessCommand {
            command: "isready".to_string(),
            response_tx,
        });

        assert!(send_result.is_ok());

        // Test that clone also works
        let (response_tx2, _response_rx2) = oneshot::channel();
        let send_result2 = command_sender_clone.send(EngineCommand::ProcessCommand {
            command: "uci".to_string(),
            response_tx: response_tx2,
        });

        assert!(send_result2.is_ok());
    }

    #[tokio::test]
    async fn test_concurrent_command_processing() {
        let engine = UCIEngine::new();
        engine.initialize().await.unwrap();

        // Test that the same engine can process multiple commands sequentially
        for _ in 0..10 {
            engine.process_command("isready").await.unwrap();
        }
    }
}
