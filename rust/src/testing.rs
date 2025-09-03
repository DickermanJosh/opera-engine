// Testing utilities and async test framework for Opera UCI Engine
//
// This module provides testing infrastructure optimized for async operations
// and UCI protocol testing with comprehensive mocking and assertion utilities.

use std::time::{Duration, Instant};
use tokio::time::timeout;
use tracing::{error, info};

use crate::error::{UCIError, UCIResult};
use crate::logging::LoggingConfig;

/// Test configuration for async operations
#[derive(Debug, Clone)]
pub struct TestConfig {
    /// Default timeout for async operations in tests
    pub default_timeout: Duration,

    /// Whether to initialize logging for tests
    pub enable_logging: bool,

    /// Whether to run performance benchmarks
    pub enable_benchmarks: bool,

    /// Maximum allowed test execution time
    pub max_test_duration: Duration,
}

impl Default for TestConfig {
    fn default() -> Self {
        Self {
            default_timeout: Duration::from_millis(5000),
            enable_logging: false,
            enable_benchmarks: false,
            max_test_duration: Duration::from_secs(30),
        }
    }
}

/// Test runtime manager for coordinating async test execution
pub struct TestRuntime {
    config: TestConfig,
    start_time: Instant,
}

impl TestRuntime {
    /// Create new test runtime with configuration
    pub fn new(config: TestConfig) -> Self {
        Self {
            config,
            start_time: Instant::now(),
        }
    }

    /// Create test runtime with logging enabled
    pub fn with_logging() -> UCIResult<Self> {
        let config = TestConfig {
            enable_logging: true,
            ..Default::default()
        };

        // Initialize test logging
        let log_config = LoggingConfig::testing();
        crate::logging::initialize_logging(log_config)?;

        Ok(Self::new(config))
    }

    /// Run async test with timeout and error handling
    pub async fn run_test<F, T>(&self, name: &str, test_fn: F) -> UCIResult<T>
    where
        F: std::future::Future<Output = UCIResult<T>>,
    {
        info!("Starting test: {}", name);
        let start = Instant::now();

        let result = timeout(self.config.default_timeout, test_fn)
            .await
            .map_err(|_| UCIError::Timeout {
                duration_ms: self.config.default_timeout.as_millis() as u64,
            })?;

        let duration = start.elapsed();

        match &result {
            Ok(_) => {
                info!("Test '{}' passed in {:?}", name, duration);
            }
            Err(error) => {
                error!("Test '{}' failed in {:?}: {}", name, duration, error);
            }
        }

        result
    }

    /// Check if test runtime is approaching timeout
    pub fn check_timeout(&self) -> UCIResult<()> {
        let elapsed = self.start_time.elapsed();
        if elapsed > self.config.max_test_duration {
            return Err(UCIError::Timeout {
                duration_ms: elapsed.as_millis() as u64,
            });
        }
        Ok(())
    }
}

/// Mock UCI command generator for testing
pub struct MockUCICommands {
    commands: Vec<String>,
    current_index: usize,
}

impl MockUCICommands {
    /// Create new mock command generator
    pub fn new() -> Self {
        Self {
            commands: Vec::new(),
            current_index: 0,
        }
    }

    /// Add command to mock sequence
    pub fn add_command(mut self, command: impl Into<String>) -> Self {
        self.commands.push(command.into());
        self
    }

    /// Add standard UCI initialization sequence
    pub fn with_uci_init(self) -> Self {
        self.add_command("uci")
            .add_command("isready")
            .add_command("ucinewgame")
    }

    /// Add position setup command
    pub fn with_position(self, fen: &str) -> Self {
        self.add_command(format!("position fen {}", fen))
    }

    /// Add go command with time control
    pub fn with_go(self, time_ms: u64) -> Self {
        self.add_command(format!("go movetime {}", time_ms))
    }

    /// Get next command in sequence
    pub fn next_command(&mut self) -> Option<&str> {
        if self.current_index < self.commands.len() {
            let command = &self.commands[self.current_index];
            self.current_index += 1;
            Some(command)
        } else {
            None
        }
    }

    /// Reset to beginning of command sequence
    pub fn reset(&mut self) {
        self.current_index = 0;
    }

    /// Get all remaining commands
    pub fn remaining_commands(&self) -> &[String] {
        &self.commands[self.current_index..]
    }
}

impl Default for MockUCICommands {
    fn default() -> Self {
        Self::new()
    }
}

/// Performance measurement utilities for testing
pub struct PerformanceMeasurer {
    measurements: Vec<(String, Duration)>,
}

impl PerformanceMeasurer {
    /// Create new performance measurer
    pub fn new() -> Self {
        Self {
            measurements: Vec::new(),
        }
    }

    /// Measure execution time of an async operation
    pub async fn measure<F, T>(&mut self, name: impl Into<String>, operation: F) -> T
    where
        F: std::future::Future<Output = T>,
    {
        let start = Instant::now();
        let result = operation.await;
        let duration = start.elapsed();

        self.measurements.push((name.into(), duration));
        result
    }

    /// Get measurement by name
    pub fn get_measurement(&self, name: &str) -> Option<Duration> {
        self.measurements
            .iter()
            .find(|(n, _)| n == name)
            .map(|(_, d)| *d)
    }

    /// Get all measurements
    pub fn get_all_measurements(&self) -> &[(String, Duration)] {
        &self.measurements
    }

    /// Calculate average duration for operations with same name
    pub fn average_duration(&self, name: &str) -> Option<Duration> {
        let matching: Vec<Duration> = self
            .measurements
            .iter()
            .filter(|(n, _)| n == name)
            .map(|(_, d)| *d)
            .collect();

        if matching.is_empty() {
            return None;
        }

        let total_nanos: u64 = matching.iter().map(|d| d.as_nanos() as u64).sum();
        let avg_nanos = total_nanos / matching.len() as u64;

        Some(Duration::from_nanos(avg_nanos))
    }

    /// Print performance summary
    pub fn print_summary(&self) {
        info!("Performance Summary:");
        for (name, duration) in &self.measurements {
            info!("  {}: {:?}", name, duration);
        }
    }
}

impl Default for PerformanceMeasurer {
    fn default() -> Self {
        Self::new()
    }
}

/// Assertion utilities for UCI testing
pub mod assertions {
    use super::*;

    /// Assert that an operation completes within expected time
    pub fn assert_timing(duration: Duration, max_expected: Duration) -> UCIResult<()> {
        if duration > max_expected {
            return Err(UCIError::Internal {
                message: format!(
                    "Operation took {:?}, expected max {:?}",
                    duration, max_expected
                ),
            });
        }
        Ok(())
    }

    /// Assert that a UCI response is valid
    pub fn assert_valid_uci_response(response: &str) -> UCIResult<()> {
        if response.is_empty() {
            return Err(UCIError::Protocol {
                message: "Empty UCI response".to_string(),
            });
        }

        // Check for common UCI response formats
        let valid_prefixes = ["bestmove", "info", "option", "uciok", "readyok", "id"];

        if !valid_prefixes
            .iter()
            .any(|prefix| response.starts_with(prefix))
        {
            return Err(UCIError::Protocol {
                message: format!("Invalid UCI response format: '{}'", response),
            });
        }

        Ok(())
    }

    /// Assert that an error is of expected type
    pub fn assert_error_type(error: &UCIError, expected_type: &str) -> UCIResult<()> {
        let error_type = match error {
            UCIError::Protocol { .. } => "Protocol",
            UCIError::Engine { .. } => "Engine",
            UCIError::Position { .. } => "Position",
            UCIError::Move { .. } => "Move",
            UCIError::Search { .. } => "Search",
            UCIError::Configuration { .. } => "Configuration",
            UCIError::Io { .. } => "Io",
            UCIError::Ffi { .. } => "Ffi",
            UCIError::Timeout { .. } => "Timeout",
            UCIError::Resource { .. } => "Resource",
            UCIError::Internal { .. } => "Internal",
        };

        if error_type != expected_type {
            return Err(UCIError::Internal {
                message: format!(
                    "Expected error type '{}', got '{}'",
                    expected_type, error_type
                ),
            });
        }

        Ok(())
    }
}

/// Helper macros for async testing
#[macro_export]
macro_rules! async_test {
    ($name:ident, $body:block) => {
        #[tokio::test]
        async fn $name() -> $crate::error::UCIResult<()> {
            use $crate::testing::TestRuntime;

            let runtime = TestRuntime::new(Default::default());
            runtime.run_test(stringify!($name), async move {
                $body
                Ok(())
            }).await
        }
    };
}

#[macro_export]
macro_rules! async_test_with_logging {
    ($name:ident, $body:block) => {
        #[tokio::test]
        async fn $name() -> $crate::error::UCIResult<()> {
            use $crate::testing::TestRuntime;

            let runtime = TestRuntime::with_logging()?;
            runtime.run_test(stringify!($name), async move {
                $body
                Ok(())
            }).await
        }
    };
}

#[macro_export]
macro_rules! benchmark_test {
    ($name:ident, $max_duration:expr, $body:block) => {
        #[tokio::test]
        async fn $name() -> $crate::error::UCIResult<()> {
            use std::time::Duration;
            use $crate::testing::{PerformanceMeasurer, TestConfig, TestRuntime};

            let config = TestConfig {
                enable_benchmarks: true,
                default_timeout: $max_duration * 2,
                ..Default::default()
            };

            let runtime = TestRuntime::new(config);
            let mut measurer = PerformanceMeasurer::new();

            runtime
                .run_test(stringify!($name), async move {
                    let result = measurer.measure(stringify!($name), async { $body }).await;

                    measurer.print_summary();

                    if let Some(duration) = measurer.get_measurement(stringify!($name)) {
                        $crate::testing::assertions::assert_timing(duration, $max_duration)?;
                    }

                    Ok(result)
                })
                .await
        }
    };
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::time::Duration;

    #[tokio::test]
    async fn test_runtime_creation() -> UCIResult<()> {
        let config = TestConfig::default();
        let _runtime = TestRuntime::new(config);
        Ok(())
    }

    #[tokio::test]
    async fn test_mock_commands() {
        let mut mock = MockUCICommands::new()
            .with_uci_init()
            .with_position("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")
            .with_go(1000);

        assert_eq!(mock.next_command(), Some("uci"));
        assert_eq!(mock.next_command(), Some("isready"));
        assert_eq!(mock.next_command(), Some("ucinewgame"));

        mock.reset();
        assert_eq!(mock.next_command(), Some("uci"));
    }

    #[tokio::test]
    async fn test_performance_measurer() {
        let mut measurer = PerformanceMeasurer::new();

        let result = measurer
            .measure("test_op", async {
                tokio::time::sleep(Duration::from_millis(10)).await;
                42
            })
            .await;

        assert_eq!(result, 42);
        assert!(measurer.get_measurement("test_op").is_some());

        let duration = measurer.get_measurement("test_op").unwrap();
        assert!(duration >= Duration::from_millis(10));
    }

    #[tokio::test]
    async fn test_assertions() -> UCIResult<()> {
        use assertions::*;

        // Test timing assertion
        assert_timing(Duration::from_millis(50), Duration::from_millis(100))?;

        // Test UCI response validation
        assert_valid_uci_response("bestmove e2e4")?;
        assert_valid_uci_response("info depth 5 score cp 25")?;

        // Test error type assertion
        let error = UCIError::Protocol {
            message: "test".to_string(),
        };
        assert_error_type(&error, "Protocol")?;

        Ok(())
    }

    async_test!(test_async_macro, {
        // Test that async_test macro works
        tokio::time::sleep(Duration::from_millis(1)).await;
    });

    benchmark_test!(test_benchmark_macro, Duration::from_millis(100), {
        tokio::time::sleep(Duration::from_millis(10)).await;
        ()
    });
}
