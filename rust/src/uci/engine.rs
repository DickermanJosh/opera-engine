// UCI Engine Main Coordinator and Command Handler
//
// This module provides the main UCIEngine struct that coordinates all UCI protocol
// operations with thread-safe state management and async command processing.

use std::sync::Arc;
use tokio::sync::{broadcast, mpsc, oneshot};
use tokio::time::{Duration, Instant};
use tracing::{debug, error, info, instrument, warn};

use crate::error::{UCIError, UCIResult};
use crate::uci::commands::{TimeControl, UCICommand};
use crate::uci::parser::ZeroCopyParser;
use crate::uci::state::{EngineConfig, EngineState, SearchContext, StateChangeEvent, UCIState};

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
        state.update_config(|cfg| *cfg = config).expect("Failed to set initial config");
        
        let (command_tx, command_rx) = mpsc::unbounded_channel();
        let (response_tx, _) = broadcast::channel(64);
        
        Self {
            state,
            parser: parking_lot::Mutex::new(ZeroCopyParser::new()),
            command_tx,
            command_rx: Some(command_rx),
            response_tx,
            id_info: EngineIdentification::default(),
            startup_time: Instant::now(),
        }
    }

    /// Initialize the engine and transition to ready state
    #[instrument(skip(self))]
    pub async fn initialize(&self) -> UCIResult<()> {
        info!("Initializing UCI engine");
        
        // Perform initialization steps
        self.state.transition_to(EngineState::Ready, "Engine initialization complete")?;
        
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
        }
        
        info!("UCI engine command processing loop ended");
        Ok(())
    }

    /// Handle internal engine commands
    async fn handle_engine_command(&self, command: EngineCommand) -> UCIResult<()> {
        match command {
            EngineCommand::ProcessCommand { command, response_tx } => {
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
                    message: "Engine shutdown requested".to_string() 
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
    async fn handle_setoption_command(
        &self, 
        name: &str, 
        value: Option<&str>
    ) -> UCIResult<()> {
        debug!(name, value, "Setting UCI option");
        
        match name.to_lowercase().as_str() {
            "hash" => {
                if let Some(value_str) = value {
                    let hash_size: u32 = value_str.parse().map_err(|_| UCIError::Protocol {
                        message: format!("Invalid hash size: {}", value_str),
                    })?;
                    
                    self.state.update_config(|cfg| {
                        cfg.hash_size_mb = hash_size.clamp(1, 2048);
                    })?;
                    
                    info!(hash_size_mb = hash_size, "Hash size updated");
                }
            }
            "threads" => {
                if let Some(value_str) = value {
                    let thread_count: u32 = value_str.parse().map_err(|_| UCIError::Protocol {
                        message: format!("Invalid thread count: {}", value_str),
                    })?;
                    
                    self.state.update_config(|cfg| {
                        cfg.thread_count = thread_count.clamp(1, 64);
                        cfg.multithread_enabled = thread_count > 1;
                    })?;
                    
                    info!(thread_count, "Thread count updated");
                }
            }
            "ponder" => {
                if let Some(value_str) = value {
                    let ponder_enabled = matches!(value_str.to_lowercase().as_str(), "true" | "1");
                    
                    self.state.update_config(|cfg| {
                        cfg.ponder_enabled = ponder_enabled;
                    })?;
                    
                    info!(ponder_enabled, "Ponder setting updated");
                }
            }
            _ => {
                warn!(name, "Unknown UCI option");
            }
        }
        
        Ok(())
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
        info!("Starting new game");
        
        // Reset engine state but keep configuration
        self.state.reset()?;
        
        // TODO: Clear hash tables and reset position
        // This will be implemented when we integrate with the C++ engine
        
        Ok(())
    }

    /// Handle position command
    async fn handle_position_command(
        &self,
        _position: crate::uci::commands::Position<'_>,
        _moves: Vec<crate::uci::commands::ChessMove<'_>>,
    ) -> UCIResult<()> {
        debug!("Setting board position");
        
        // TODO: Set board position and apply moves
        // This will be implemented when we integrate with the C++ engine
        
        Ok(())
    }

    /// Handle go command to start search
    async fn handle_go_command(&self, time_control: TimeControl) -> UCIResult<()> {
        info!(time_control = ?time_control, "Starting search");
        
        let search_context = SearchContext {
            start_time: std::time::Instant::now(),
            time_control,
            max_depth: None,
            max_nodes: None,
            is_infinite: false,
            is_ponder: false,
        };
        
        // Start search
        self.state.start_search(search_context)?;
        
        // TODO: Actually perform the search
        // For now, simulate a quick search and return a dummy move
        tokio::spawn({
            let state = Arc::clone(&self.state);
            let response_tx = self.response_tx.clone();
            
            async move {
                // Simulate search time
                tokio::time::sleep(Duration::from_millis(100)).await;
                
                // Complete search
                if let Err(e) = state.complete_search(1000) {
                    error!(error = ?e, "Failed to complete search");
                    return;
                }
                
                // Send best move (dummy for now)
                let _ = response_tx.send("bestmove e2e4".to_string());
            }
        });
        
        Ok(())
    }

    /// Handle stop command
    async fn handle_stop_command(&self) -> UCIResult<()> {
        self.stop_search().await
    }

    /// Handle ponder hit command
    async fn handle_ponderhit_command(&self) -> UCIResult<()> {
        debug!("Ponder hit received");
        
        let current_state = self.state.current_state();
        if current_state == EngineState::Pondering {
            self.state.transition_to(EngineState::Searching, "Ponder hit - converting to search")?;
        }
        
        Ok(())
    }

    /// Handle quit command
    async fn handle_quit_command(&self) -> UCIResult<()> {
        info!("Quit command received");
        self.shutdown().await
    }

    /// Stop current search operation
    async fn stop_search(&self) -> UCIResult<()> {
        let current_state = self.state.current_state();
        
        if current_state.is_computing() {
            info!("Stopping current search");
            
            // TODO: Signal C++ engine to stop search
            
            // Complete search with current results
            self.state.complete_search(0)?;
            
            // Send stop confirmation (best move should have been sent already)
            self.send_response("bestmove (none)")?;
        } else {
            debug!(state = ?current_state, "Stop command received but not searching");
        }
        
        Ok(())
    }

    /// Shutdown the engine gracefully
    async fn shutdown(&self) -> UCIResult<()> {
        info!("Shutting down UCI engine");
        
        self.state.transition_to(EngineState::Stopping, "Engine shutdown requested")?;
        
        // Stop any ongoing search
        if self.state.current_state().is_computing() {
            let _ = self.stop_search().await;
        }
        
        Ok(())
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
        let config = self.state.config();
        
        // Hash size option
        self.send_response(&format!(
            "option name Hash type spin default {} min 1 max 2048",
            config.hash_size_mb
        ))?;
        
        // Thread count option  
        self.send_response(&format!(
            "option name Threads type spin default {} min 1 max 64",
            config.thread_count
        ))?;
        
        // Ponder option
        self.send_response(&format!(
            "option name Ponder type check default {}",
            config.ponder_enabled
        ))?;
        
        // Analysis mode option
        self.send_response(&format!(
            "option name UCI_AnalyseMode type check default {}",
            config.analysis_mode
        ))?;
        
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
        while let Ok(response) = tokio::time::timeout(Duration::from_millis(10), responses.recv()).await {
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
        engine.process_command("setoption name Hash value 64").await.unwrap();
        let config = engine.state.config();
        assert_eq!(config.hash_size_mb, 64);
        
        // Test threads option
        engine.process_command("setoption name Threads value 4").await.unwrap();
        let config = engine.state.config();
        assert_eq!(config.thread_count, 4);
        assert!(config.multithread_enabled);
        
        // Test ponder option
        engine.process_command("setoption name Ponder value true").await.unwrap();
        let config = engine.state.config();
        assert!(config.ponder_enabled);
    }

    #[tokio::test]
    async fn test_go_command() {
        let engine = UCIEngine::new();
        engine.initialize().await.unwrap();
        
        let mut responses = engine.subscribe_responses();
        let mut state_changes = engine.subscribe_state_changes();
        
        engine.process_command("go movetime 1000").await.unwrap();
        
        // Should transition to searching
        let state_change = tokio::time::timeout(Duration::from_millis(500), state_changes.recv()).await.unwrap().unwrap();
        assert_eq!(state_change.to, EngineState::Searching);
        
        // Should eventually get a best move response
        let response = tokio::time::timeout(Duration::from_millis(500), responses.recv()).await.unwrap().unwrap();
        assert!(response.starts_with("bestmove"));
        
        // Should return to ready state
        let state_change = tokio::time::timeout(Duration::from_millis(500), state_changes.recv()).await.unwrap().unwrap();
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
        let state_change = tokio::time::timeout(Duration::from_millis(500), state_changes.recv()).await.unwrap().unwrap();
        assert_eq!(state_change.to, EngineState::Searching);
        
        // Stop the search
        engine.process_command("stop").await.unwrap();
        
        // Should return to ready
        let state_change = tokio::time::timeout(Duration::from_millis(500), state_changes.recv()).await.unwrap().unwrap();
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
        let state_change = tokio::time::timeout(Duration::from_millis(500), state_changes.recv()).await.unwrap().unwrap();
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