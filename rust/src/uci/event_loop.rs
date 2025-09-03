// UCI Async I/O Command Processing Event Loop
//
// This module implements the main async event loop for UCI protocol processing
// using tokio::select! for responsive command handling with proper prioritization
// and graceful shutdown.

use std::sync::Arc;
use tokio::io::{AsyncBufReadExt, AsyncWriteExt, BufReader};
use tokio::sync::{broadcast, oneshot};
use tokio::time::{timeout, Duration, Instant};
use tokio::{select, signal};
use tracing::{debug, error, info, instrument, warn};

use crate::error::{UCIError, UCIResult};
use crate::uci::engine::UCIEngine;
use crate::uci::parser::ZeroCopyParser;
use crate::uci::sanitizer::InputSanitizer;

/// Main UCI event loop coordinator with async I/O processing
pub struct UCIEventLoop {
    /// Input reader for stdin commands
    stdin_reader: BufReader<tokio::io::Stdin>,

    /// Output writer for stdout responses
    stdout_writer: tokio::io::Stdout,

    /// UCI engine instance
    engine: Arc<UCIEngine>,

    /// Command parser with input validation
    parser: ZeroCopyParser,

    /// Input sanitizer for security
    sanitizer: InputSanitizer,

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
        let stdin = tokio::io::stdin();
        let stdout = tokio::io::stdout();
        let stdin_reader = BufReader::with_capacity(config.input_buffer_size, stdin);

        // Subscribe to engine responses
        let response_rx = engine.subscribe_responses();

        Ok(Self {
            stdin_reader,
            stdout_writer: stdout,
            engine,
            parser: ZeroCopyParser::new(),
            sanitizer: InputSanitizer::default(),
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

        // Initialize engine
        self.engine
            .initialize()
            .await
            .map_err(|e| UCIError::Engine {
                message: format!("Failed to initialize engine: {}", e),
            })?;

        let mut input_buffer = String::with_capacity(self.config.input_buffer_size);
        let mut graceful_shutdown = false;

        loop {
            input_buffer.clear();

            select! {
                // Handle stdin input with highest priority
                result = self.stdin_reader.read_line(&mut input_buffer) => {
                    match result {
                        Ok(0) => {
                            info!("EOF received on stdin - initiating graceful shutdown");
                            graceful_shutdown = true;
                            break;
                        }
                        Ok(_) => {
                            if let Err(e) = self.process_input_command(&input_buffer).await {
                                error!(error = %e, "Failed to process input command");
                                // Continue processing despite errors
                            }
                        }
                        Err(e) => {
                            error!(error = %e, "Failed to read from stdin");
                            return Err(UCIError::Io {
                                message: format!("Stdin read error: {}", e)
                            });
                        }
                    }
                }

                // Handle engine responses
                result = self.response_rx.recv() => {
                    match result {
                        Ok(response) => {
                            if let Err(e) = self.send_response(&response).await {
                                error!(error = %e, response = %response, "Failed to send response");
                            }
                        }
                        Err(broadcast::error::RecvError::Closed) => {
                            info!("Engine response channel closed");
                            break;
                        }
                        Err(broadcast::error::RecvError::Lagged(skipped)) => {
                            warn!(skipped_responses = skipped, "Response buffer lagged - some responses may have been dropped");
                        }
                    }
                }

                // Handle shutdown signal
                _ = async {
                    if let Some(shutdown_rx) = self.shutdown_rx.take() {
                        let _ = shutdown_rx.await;
                    } else {
                        std::future::pending::<()>().await;
                    }
                }, if self.shutdown_rx.is_some() => {
                    info!("Shutdown signal received");
                    graceful_shutdown = true;
                    break;
                }

                // Periodic maintenance and monitoring
                _ = tokio::time::sleep(Duration::from_secs(1)), if self.config.enable_monitoring => {
                    self.update_stats();

                    // Log performance metrics periodically
                    if self.stats.commands_processed % 100 == 0 && self.stats.commands_processed > 0 {
                        debug!(
                            commands = self.stats.commands_processed,
                            responses = self.stats.responses_sent,
                            avg_time_ms = self.stats.avg_command_time_ms,
                            "Event loop performance metrics"
                        );
                    }
                }
            }

            // Check for quit command processing
            if self.should_shutdown() {
                info!("Quit command processed - initiating shutdown");
                break;
            }
        }

        // Graceful shutdown sequence
        if graceful_shutdown {
            self.graceful_shutdown().await?;
        }

        info!(
            uptime = ?self.stats.uptime,
            commands_processed = self.stats.commands_processed,
            responses_sent = self.stats.responses_sent,
            "UCI event loop shutdown complete"
        );

        Ok(())
    }

    /// Process a single input command with timeout and error handling
    #[instrument(skip(self, input))]
    async fn process_input_command(&mut self, input: &str) -> UCIResult<()> {
        let command_start = Instant::now();

        // Sanitize and validate input
        let sanitized = self
            .sanitizer
            .sanitize_string(input)
            .map_err(|e| UCIError::Protocol {
                message: format!("Input sanitization failed: {}", e),
            })?;

        if sanitized.is_empty() {
            return Ok(()); // Skip empty lines
        }

        debug!(command = %sanitized, "Processing UCI command");

        // Parse command with timeout
        let parse_result = timeout(
            Duration::from_millis(100), // Quick parse timeout
            async { self.parser.parse_command(&sanitized) },
        )
        .await
        .map_err(|_| UCIError::Timeout { duration_ms: 100 })?;

        match parse_result {
            Ok(_command) => {
                // Process command with timeout
                let process_result = timeout(
                    Duration::from_millis(self.config.command_timeout_ms),
                    self.engine.process_command(&sanitized),
                )
                .await;

                match process_result {
                    Ok(Ok(())) => {
                        // Command processed successfully
                        let elapsed = command_start.elapsed();
                        self.update_command_stats(elapsed);

                        debug!(
                            command = %sanitized,
                            processing_time_ms = elapsed.as_millis(),
                            "Command processed successfully"
                        );
                    }
                    Ok(Err(e)) => {
                        error!(
                            command = %sanitized,
                            error = %e,
                            "Engine command processing failed"
                        );
                        return Err(e);
                    }
                    Err(_) => {
                        self.stats.command_timeouts += 1;
                        warn!(
                            command = %sanitized,
                            timeout_ms = self.config.command_timeout_ms,
                            "Command processing timed out"
                        );
                        return Err(UCIError::Timeout {
                            duration_ms: self.config.command_timeout_ms,
                        });
                    }
                }
            }
            Err(e) => {
                warn!(
                    input = %sanitized,
                    error = %e,
                    "Command parsing failed"
                );
                // Send error info to GUI
                let error_response = format!("info string ERROR: Invalid command: {}", e);
                self.send_response(&error_response).await?;
            }
        }

        Ok(())
    }

    /// Send response to stdout with error handling
    #[instrument(skip(self))]
    async fn send_response(&mut self, response: &str) -> UCIResult<()> {
        let response_with_newline = format!("{}\n", response);

        match timeout(
            Duration::from_millis(self.config.response_timeout_ms),
            self.stdout_writer
                .write_all(response_with_newline.as_bytes()),
        )
        .await
        {
            Ok(Ok(())) => {
                // Ensure immediate delivery
                if let Err(e) = self.stdout_writer.flush().await {
                    error!(error = %e, "Failed to flush stdout");
                    return Err(UCIError::Io {
                        message: format!("Stdout flush error: {}", e),
                    });
                }

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

        // Send final responses if any are queued
        let shutdown_deadline =
            Instant::now() + Duration::from_millis(self.config.shutdown_timeout_ms);

        while Instant::now() < shutdown_deadline {
            select! {
                result = self.response_rx.recv() => {
                    match result {
                        Ok(response) => {
                            if let Err(e) = self.send_response(&response).await {
                                warn!(error = %e, "Failed to send final response during shutdown");
                            }
                        }
                        Err(_) => break, // Channel closed
                    }
                }
                _ = tokio::time::sleep(Duration::from_millis(100)) => {
                    // Check for more responses periodically
                }
            }
        }

        // Final stdout flush
        if let Err(e) = self.stdout_writer.flush().await {
            warn!(error = %e, "Failed final stdout flush during shutdown");
        }

        info!("Graceful shutdown sequence complete");
        Ok(())
    }

    /// Update command processing statistics
    fn update_command_stats(&mut self, processing_time: Duration) {
        self.stats.commands_processed += 1;

        // Update rolling average processing time
        let new_time_ms = processing_time.as_millis() as f64;
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

    // Create shutdown signal
    let (shutdown_tx, shutdown_rx) = oneshot::channel();

    // Setup signal handling
    tokio::spawn(async move {
        match signal::ctrl_c().await {
            Ok(()) => {
                info!("Ctrl+C received - sending shutdown signal");
                let _ = shutdown_tx.send(());
            }
            Err(err) => {
                error!(error = %err, "Failed to setup signal handler");
            }
        }
    });

    // Create and run event loop
    let mut event_loop =
        UCIEventLoop::with_config(engine, config)?.with_shutdown_signal(shutdown_rx);

    event_loop.run().await
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
