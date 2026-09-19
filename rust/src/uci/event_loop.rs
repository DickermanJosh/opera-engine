// UCI Async I/O Command Processing Event Loop
//
// This module implements the main async event loop for UCI protocol processing
// using tokio::select! for responsive command handling with proper prioritization
// and graceful shutdown.

use std::sync::Arc;
use tokio::io::AsyncWriteExt;
use tokio::sync::{broadcast, oneshot};
use tokio::time::{timeout, Duration, Instant};
use tokio::{select, signal};
use tracing::{debug, error, info, instrument};

use crate::error::{UCIError, UCIResult};
use crate::uci::engine::UCIEngine;

/// Main UCI event loop coordinator with async I/O processing
pub struct UCIEventLoop {
    /// Output writer for stdout responses
    stdout_writer: tokio::io::Stdout,

    /// UCI engine instance
    engine: Arc<UCIEngine>,

    /// Response receiver from engine
    response_rx: broadcast::Receiver<String>,

    /// Shutdown signal receiver
    shutdown_rx: Option<oneshot::Receiver<()>>,

    /// Performance statistics
    stats: EventLoopStats,

    /// Configuration
    config: EventLoopConfig,
}

/// Event loop configuration options
#[derive(Debug, Clone)]
pub struct EventLoopConfig {
    /// Maximum time to wait for engine response
    pub response_timeout_ms: u64,

    /// Maximum command processing time before warning
    pub command_timeout_ms: u64,

    /// Buffer size for input lines
    pub input_buffer_size: usize,

    /// Enable performance monitoring
    pub enable_monitoring: bool,

    /// Graceful shutdown timeout
    pub shutdown_timeout_ms: u64,
}

impl Default for EventLoopConfig {
    fn default() -> Self {
        Self {
            response_timeout_ms: 5000, // 5 second response timeout
            command_timeout_ms: 1000,  // 1 second command timeout
            input_buffer_size: 8192,   // 8KB input buffer
            enable_monitoring: true,
            shutdown_timeout_ms: 3000, // 3 second shutdown timeout
        }
    }
}

/// Performance and diagnostic statistics
#[derive(Debug)]
pub struct EventLoopStats {
    /// Total commands processed
    pub commands_processed: u64,

    /// Total responses sent
    pub responses_sent: u64,

    /// Commands that timed out
    pub command_timeouts: u64,

    /// Average command processing time
    pub avg_command_time_ms: f64,

    /// Peak memory usage
    pub peak_memory_kb: u64,

    /// Event loop uptime
    pub uptime: Duration,

    /// Start time
    pub start_time: Instant,
}

impl Default for EventLoopStats {
    fn default() -> Self {
        Self {
            commands_processed: 0,
            responses_sent: 0,
            command_timeouts: 0,
            avg_command_time_ms: 0.0,
            peak_memory_kb: 0,
            uptime: Duration::from_secs(0),
            start_time: Instant::now(),
        }
    }
}

impl UCIEventLoop {
    /// Create a new UCI event loop with default configuration
    pub fn new(engine: Arc<UCIEngine>) -> UCIResult<Self> {
        Self::with_config(engine, EventLoopConfig::default())
    }

    /// Create a new UCI event loop with custom configuration
    pub fn with_config(engine: Arc<UCIEngine>, config: EventLoopConfig) -> UCIResult<Self> {
        let stdout = tokio::io::stdout();

        // Subscribe to engine responses
        let response_rx = engine.subscribe_responses();

        Ok(Self {
            stdout_writer: stdout,
            engine,
            response_rx,
            shutdown_rx: None,
            stats: EventLoopStats {
                start_time: Instant::now(),
                ..Default::default()
            },
            config,
        })
    }

    /// Set shutdown signal receiver
    pub fn with_shutdown_signal(mut self, shutdown_rx: oneshot::Receiver<()>) -> Self {
        self.shutdown_rx = Some(shutdown_rx);
        self
    }

    /// Run the main event loop until shutdown
    #[instrument(skip(self))]
    pub async fn run(&mut self) -> UCIResult<()> {
        info!("Starting UCI event loop");

        let shutdown_signal = shutdown_signal().map_err(|e| UCIError::Io {
            message: format!("Failed to register shutdown signals: {e}"),
        })?;
        tokio::pin!(shutdown_signal);

        // Initialize engine
        self.engine
            .initialize()
            .await
            .map_err(|e| UCIError::Engine {
                message: format!("Failed to initialize engine: {}", e),
            })?;

        let (input_tx, mut input_rx) = tokio::sync::mpsc::channel::<Result<String, String>>(64);
        let input_capacity = self.config.input_buffer_size.max(1);
        // Tokio stdin uses an uncancellable blocking task and can keep runtime shutdown
        // alive after quit. A dedicated reader is detached; process exit closes its fd.
        std::thread::spawn(move || {
            use std::io::{BufRead, BufReader};
            let mut reader = BufReader::with_capacity(input_capacity, std::io::stdin());
            loop {
                let mut bytes = Vec::new();
                let mut oversized = false;
                loop {
                    let chunk = match reader.fill_buf() {
                        Ok(c) => c,
                        Err(e) => {
                            let _ = input_tx.blocking_send(Err(e.to_string()));
                            return;
                        }
                    };
                    if chunk.is_empty() {
                        if oversized {
                            let _ =
                                input_tx.blocking_send(Err("command exceeds 4096 bytes".into()));
                        } else if !bytes.is_empty() {
                            let _ = input_tx
                                .blocking_send(String::from_utf8(bytes).map_err(|e| e.to_string()));
                        }
                        return;
                    }
                    let length = chunk
                        .iter()
                        .position(|b| *b == b'\n')
                        .map(|i| i + 1)
                        .unwrap_or(chunk.len());
                    let ended = chunk[length - 1] == b'\n';
                    if bytes.len() + length <= 4096 && !oversized {
                        bytes.extend_from_slice(&chunk[..length]);
                    } else {
                        oversized = true;
                    }
                    reader.consume(length);
                    if ended {
                        break;
                    }
                }
                let line = if oversized {
                    Err("command exceeds 4096 bytes".into())
                } else {
                    String::from_utf8(bytes).map_err(|e| e.to_string())
                };
                if input_tx.blocking_send(line).is_err() {
                    return;
                }
            }
        });
        let result = async {
            loop {
                select! {
                    biased;
                    signal = &mut shutdown_signal => {
                        signal.map_err(|e| UCIError::Io { message: e.to_string() })?;
                        break Ok(());
                    }
                    _ = async {
                        match self.shutdown_rx.as_mut() {
                            Some(rx) => { let _ = rx.await; }
                            None => std::future::pending::<()>().await,
                        }
                    } => break Ok(()),
                    result = self.response_rx.recv() => {
                        match result {
                            Ok(line) => self.send_response(&line).await?,
                            Err(broadcast::error::RecvError::Lagged(_)) => {
                                return Err(UCIError::Io {
                                    message: "protocol output queue overflow".into(),
                                });
                            }
                            Err(_) => break Ok(()),
                        }
                    }
                    line = input_rx.recv() => {
                        match line {
                            Some(Ok(line)) => {
                                if let Err(e) = self.process_input_command(&line).await {
                                    self.send_response(&format!("info string ERROR: {e}")).await?;
                                }
                                if self.should_shutdown() { break Ok(()); }
                            }
                            Some(Err(e)) => self.send_response(&format!("info string ERROR: {e}")).await?,
                            None => break Ok(()),
                        }
                    }
                }
                self.update_stats();
            }
        }
        .await;
        // Join the worker on every exit path, including an output or signal error.
        let stopped = self.engine.process_command("quit").await;
        result?;
        stopped?;
        self.graceful_shutdown().await
    }

    /// Process a single input command with timeout and error handling
    #[instrument(skip(self, input))]
    async fn process_input_command(&mut self, input: &str) -> UCIResult<()> {
        let command_start = Instant::now();
        if input.trim().is_empty() {
            return Ok(());
        }
        self.engine.process_command(input).await?;
        self.update_command_stats(command_start.elapsed());
        Ok(())
    }

    /// Send response to stdout with error handling
    #[instrument(skip(self))]
    async fn send_response(&mut self, response: &str) -> UCIResult<()> {
        let response_with_newline = format!("{}\n", response);

        match timeout(
            Duration::from_millis(self.config.response_timeout_ms),
            async {
                self.stdout_writer
                    .write_all(response_with_newline.as_bytes())
                    .await?;
                self.stdout_writer.flush().await
            },
        )
        .await
        {
            Ok(Ok(())) => {
                self.stats.responses_sent += 1;
                debug!(response = %response, "Response sent");
                Ok(())
            }
            Ok(Err(e)) => {
                error!(error = %e, response = %response, "Failed to write response");
                Err(UCIError::Io {
                    message: format!("Stdout write error: {}", e),
                })
            }
            Err(_) => {
                error!(
                    timeout_ms = self.config.response_timeout_ms,
                    response = %response,
                    "Response write timed out"
                );
                Err(UCIError::Timeout {
                    duration_ms: self.config.response_timeout_ms,
                })
            }
        }
    }

    /// Check if the engine has processed a quit command
    fn should_shutdown(&self) -> bool {
        // Check engine state for quit processing
        matches!(
            self.engine.state(),
            crate::uci::state::EngineState::Stopping
        )
    }

    /// Perform graceful shutdown sequence
    #[instrument(skip(self))]
    async fn graceful_shutdown(&mut self) -> UCIResult<()> {
        info!("Starting graceful shutdown sequence");

        // Search has joined, so all final responses are already queued.
        timeout(
            Duration::from_millis(self.config.shutdown_timeout_ms),
            async {
                while let Ok(line) = self.response_rx.try_recv() {
                    self.send_response(&line).await?;
                }
                self.stdout_writer.flush().await.map_err(|e| UCIError::Io {
                    message: e.to_string(),
                })
            },
        )
        .await
        .map_err(|_| UCIError::Timeout {
            duration_ms: self.config.shutdown_timeout_ms,
        })?
    }

    /// Update command processing statistics
    fn update_command_stats(&mut self, processing_time: Duration) {
        self.stats.commands_processed += 1;

        // Update rolling average processing time
        let new_time_ms = processing_time.as_secs_f64() * 1000.0;
        let count = self.stats.commands_processed as f64;
        self.stats.avg_command_time_ms =
            ((self.stats.avg_command_time_ms * (count - 1.0)) + new_time_ms) / count;
    }

    /// Update general statistics
    fn update_stats(&mut self) {
        self.stats.uptime = self.stats.start_time.elapsed();

        // Update memory usage if monitoring is enabled
        if self.config.enable_monitoring {
            // Simple memory monitoring - in production this could use more sophisticated tracking
            let memory_kb = self.estimate_memory_usage();
            if memory_kb > self.stats.peak_memory_kb {
                self.stats.peak_memory_kb = memory_kb;
            }
        }
    }

    /// Estimate current memory usage (simplified implementation)
    fn estimate_memory_usage(&self) -> u64 {
        // Basic estimation - in production this would use proper memory profiling
        (self.config.input_buffer_size + 4096) as u64 // Buffer + overhead estimate
    }

    /// Get current performance statistics
    pub fn stats(&self) -> &EventLoopStats {
        &self.stats
    }
}

/// Utility function to create and run a UCI event loop with signal handling
#[instrument]
pub async fn run_uci_event_loop(config: EventLoopConfig) -> UCIResult<()> {
    info!("Initializing UCI event loop with signal handling");

    // Create engine instance
    let engine = Arc::new(UCIEngine::new());

    // Create and run event loop
    let mut event_loop = UCIEventLoop::with_config(engine, config)?;

    event_loop.run().await
}

// Install Unix handlers before processing commands so SIGTERM (including Docker
// stop) and SIGINT follow the same worker-join/output-drain path as UCI quit.
fn shutdown_signal() -> std::io::Result<impl std::future::Future<Output = std::io::Result<()>>> {
    #[cfg(unix)]
    {
        let mut interrupt = signal::unix::signal(signal::unix::SignalKind::interrupt())?;
        let mut terminate = signal::unix::signal(signal::unix::SignalKind::terminate())?;
        Ok(async move {
            select! {
                _ = interrupt.recv() => Ok(()),
                _ = terminate.recv() => Ok(()),
            }
        })
    }
    #[cfg(not(unix))]
    {
        Ok(signal::ctrl_c())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    /// Helper to create a test event loop
    async fn create_test_event_loop() -> UCIEventLoop {
        let engine = Arc::new(UCIEngine::new());
        engine
            .initialize()
            .await
            .expect("Engine initialization should succeed");

        let config = EventLoopConfig {
            response_timeout_ms: 1000,
            command_timeout_ms: 500,
            input_buffer_size: 1024,
            enable_monitoring: false,
            shutdown_timeout_ms: 1000,
        };

        UCIEventLoop::with_config(engine, config).expect("Event loop creation should succeed")
    }

    #[tokio::test]
    async fn test_event_loop_creation() {
        let event_loop = create_test_event_loop().await;

        // Verify initial state
        assert_eq!(event_loop.stats.commands_processed, 0);
        assert_eq!(event_loop.stats.responses_sent, 0);
        assert_eq!(event_loop.config.response_timeout_ms, 1000);
    }

    #[tokio::test]
    async fn test_config_default() {
        let config = EventLoopConfig::default();

        assert_eq!(config.response_timeout_ms, 5000);
        assert_eq!(config.command_timeout_ms, 1000);
        assert_eq!(config.input_buffer_size, 8192);
        assert!(config.enable_monitoring);
    }

    #[tokio::test]
    async fn test_stats_initialization() {
        let event_loop = create_test_event_loop().await;

        assert_eq!(event_loop.stats().commands_processed, 0);
        assert_eq!(event_loop.stats().responses_sent, 0);
        assert_eq!(event_loop.stats().command_timeouts, 0);
        assert_eq!(event_loop.stats().avg_command_time_ms, 0.0);
    }

    #[tokio::test]
    async fn test_command_stats_update() {
        let mut event_loop = create_test_event_loop().await;

        // Simulate command processing
        event_loop.update_command_stats(Duration::from_millis(50));
        event_loop.update_command_stats(Duration::from_millis(100));

        assert_eq!(event_loop.stats.commands_processed, 2);
        assert_eq!(event_loop.stats.avg_command_time_ms, 75.0);
    }

    #[tokio::test]
    async fn test_memory_estimation() {
        let event_loop = create_test_event_loop().await;

        let memory = event_loop.estimate_memory_usage();
        assert!(memory > 0);
        assert!(memory >= event_loop.config.input_buffer_size as u64);
    }

    #[tokio::test]
    async fn test_shutdown_signal_setup() {
        let engine = Arc::new(UCIEngine::new());
        let (shutdown_tx, shutdown_rx) = oneshot::channel();

        let event_loop = UCIEventLoop::new(engine)
            .expect("Event loop creation should succeed")
            .with_shutdown_signal(shutdown_rx);

        // Trigger shutdown
        shutdown_tx.send(()).expect("Should send shutdown signal");

        // Event loop should be configured with shutdown signal
        assert!(event_loop.shutdown_rx.is_some());
    }

    #[tokio::test]
    async fn test_response_formatting() {
        let _event_loop = create_test_event_loop().await;

        // Test that responses are properly formatted
        // Note: This test is limited because we can't easily test actual stdout writing
        // In a real implementation, we might use dependency injection for testability

        let response = "readyok";
        let formatted = format!("{}\n", response);
        assert_eq!(formatted, "readyok\n");
    }
}

#[cfg(test)]
#[path = "event_loop_tests.rs"]
mod integration_tests;
