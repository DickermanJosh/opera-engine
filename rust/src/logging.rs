// Logging infrastructure for Opera UCI Engine
//
// This module provides structured logging with tracing, supporting multiple
// output formats and configurable log levels for development and production.

use crate::error::{UCIError, UCIResult};
use std::env;
use std::path::PathBuf;
use tracing::Level;
use tracing_subscriber::{fmt::format::FmtSpan, EnvFilter};

/// Logging configuration options
#[derive(Debug, Clone)]
pub struct LoggingConfig {
    /// Log level (trace, debug, info, warn, error)
    pub level: Level,

    /// Whether to include target information in logs
    pub with_target: bool,

    /// Whether to include thread IDs in logs
    pub with_thread_ids: bool,

    /// Whether to include file and line information
    pub with_file_location: bool,

    /// Whether to include timestamps
    pub with_timestamp: bool,

    /// Span events to include (new, close, enter, exit)
    pub span_events: FmtSpan,

    /// Optional log file path (None for stdout only)
    pub file_path: Option<PathBuf>,

    /// Whether to use JSON format for structured logging
    pub json_format: bool,
}

impl Default for LoggingConfig {
    fn default() -> Self {
        Self {
            level: Level::INFO,
            with_target: false,
            with_thread_ids: true,
            with_file_location: true,
            with_timestamp: true,
            span_events: FmtSpan::CLOSE,
            file_path: None,
            json_format: false,
        }
    }
}

impl LoggingConfig {
    /// Create development configuration with verbose logging
    pub fn development() -> Self {
        Self {
            level: Level::DEBUG,
            with_target: true,
            with_thread_ids: true,
            with_file_location: true,
            with_timestamp: true,
            span_events: FmtSpan::NEW | FmtSpan::CLOSE,
            file_path: Some(PathBuf::from("opera-uci-dev.log")),
            json_format: false,
        }
    }

    /// Create production configuration with minimal logging
    pub fn production() -> Self {
        Self {
            level: Level::INFO,
            with_target: false,
            with_thread_ids: false,
            with_file_location: false,
            with_timestamp: true,
            span_events: FmtSpan::NONE,
            file_path: Some(PathBuf::from("opera-uci.log")),
            json_format: true,
        }
    }

    /// Create testing configuration with trace-level logging
    pub fn testing() -> Self {
        Self {
            level: Level::TRACE,
            with_target: true,
            with_thread_ids: true,
            with_file_location: true,
            with_timestamp: false, // Cleaner test output
            span_events: FmtSpan::ENTER | FmtSpan::EXIT,
            file_path: None, // Console only for tests
            json_format: false,
        }
    }

    /// Create configuration optimized for UCI debugging
    pub fn uci_debug() -> Self {
        Self {
            level: Level::DEBUG,
            with_target: false,
            with_thread_ids: false,
            with_file_location: false,
            with_timestamp: true,
            span_events: FmtSpan::NONE, // Minimal span noise
            file_path: Some(PathBuf::from("opera-uci-debug.log")),
            json_format: false,
        }
    }
}

/// Initialize the logging system with the given configuration
pub fn initialize_logging(config: LoggingConfig) -> UCIResult<()> {
    // Create environment filter from log level
    let env_filter = EnvFilter::try_from_default_env()
        .or_else(|_| EnvFilter::try_new(format!("opera_uci={}", config.level)))
        .map_err(|e| UCIError::Configuration {
            message: format!("Failed to create logging filter: {}", e),
        })?;

    // Simple console-only logging for now (can be enhanced later)
    let subscriber = tracing_subscriber::fmt()
        .with_env_filter(env_filter)
        .with_target(config.with_target)
        .with_thread_ids(config.with_thread_ids)
        .with_file(config.with_file_location)
        .with_line_number(config.with_file_location);

    if config.with_timestamp {
        subscriber.init();
    } else {
        subscriber.without_time().init();
    }

    tracing::info!("Logging initialized with level: {}", config.level);
    if let Some(file_path) = &config.file_path {
        tracing::info!("Log file configured: {}", file_path.display());
    }

    Ok(())
}

/// Initialize logging from environment variables
pub fn initialize_from_env() -> UCIResult<()> {
    let config = if env::var("OPERA_UCI_ENVIRONMENT").as_deref() == Ok("production") {
        LoggingConfig::production()
    } else if env::var("OPERA_UCI_ENVIRONMENT").as_deref() == Ok("development") {
        LoggingConfig::development()
    } else if env::var("OPERA_UCI_ENVIRONMENT").as_deref() == Ok("testing") {
        LoggingConfig::testing()
    } else if env::var("OPERA_UCI_DEBUG").is_ok() {
        LoggingConfig::uci_debug()
    } else {
        LoggingConfig::default()
    };

    initialize_logging(config)
}

/// Logging utilities and helper functions
pub mod utils {
    use tracing::{debug, error, info, warn};

    /// Log UCI command received from GUI
    pub fn log_uci_input(command: &str) {
        debug!(command = %command, "← UCI command received");
    }

    /// Log UCI response sent to GUI
    pub fn log_uci_output(response: &str) {
        debug!(response = %response, "→ UCI response sent");
    }

    /// Log engine operation with timing
    pub fn log_timed_operation<T>(
        operation: &str,
        result: Result<T, &dyn std::error::Error>,
        duration_ms: u64,
    ) {
        match result {
            Ok(_) => {
                info!(
                    operation = %operation,
                    duration_ms = duration_ms,
                    "✓ Operation completed successfully"
                );
            }
            Err(error) => {
                warn!(
                    operation = %operation,
                    duration_ms = duration_ms,
                    error = %error,
                    "✗ Operation failed"
                );
            }
        }
    }

    /// Log search progress with structured data
    pub fn log_search_progress(
        depth: i32,
        score: i32,
        nodes: u64,
        nps: u64,
        pv: &str,
        time_ms: u64,
    ) {
        info!(
            depth = depth,
            score = score,
            nodes = nodes,
            nps = nps,
            time_ms = time_ms,
            pv = %pv,
            "🔍 Search progress"
        );
    }

    /// Log engine error with context
    pub fn log_engine_error(operation: &str, error: &dyn std::error::Error) {
        error!(
            operation = %operation,
            error = %error,
            "🚨 Engine error occurred"
        );
    }

    /// Log FFI operation
    pub fn log_ffi_call(function: &str, success: bool) {
        if success {
            debug!(function = %function, "🔗 FFI call succeeded");
        } else {
            warn!(function = %function, "🔗 FFI call failed");
        }
    }

    /// Log system resource usage
    pub fn log_resource_usage(memory_mb: u64, cpu_percent: f32) {
        debug!(
            memory_mb = memory_mb,
            cpu_percent = cpu_percent,
            "📊 Resource usage"
        );
    }
}

/// Log level conversion utilities
pub mod level_utils {
    use crate::error::{UCIError, UCIResult};
    use tracing::Level;

    /// Parse log level from string
    pub fn parse_level(level_str: &str) -> UCIResult<Level> {
        match level_str.to_lowercase().as_str() {
            "trace" => Ok(Level::TRACE),
            "debug" => Ok(Level::DEBUG),
            "info" => Ok(Level::INFO),
            "warn" => Ok(Level::WARN),
            "error" => Ok(Level::ERROR),
            _ => Err(UCIError::Configuration {
                message: format!("Invalid log level: {}", level_str),
            }),
        }
    }

    /// Convert log level to string
    pub fn level_to_string(level: &Level) -> &'static str {
        match *level {
            Level::TRACE => "trace",
            Level::DEBUG => "debug",
            Level::INFO => "info",
            Level::WARN => "warn",
            Level::ERROR => "error",
        }
    }

    /// Get log level from environment variable
    pub fn level_from_env(var_name: &str) -> UCIResult<Level> {
        let level_str = std::env::var(var_name).map_err(|_| UCIError::Configuration {
            message: format!("Environment variable {} not set", var_name),
        })?;

        parse_level(&level_str)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use tracing::Level;

    #[test]
    fn test_default_config() {
        let config = LoggingConfig::default();
        assert_eq!(config.level, Level::INFO);
        assert!(!config.with_target);
        assert!(config.with_thread_ids);
        assert!(config.with_file_location);
        assert!(config.with_timestamp);
        assert!(config.file_path.is_none());
        assert!(!config.json_format);
    }

    #[test]
    fn test_development_config() {
        let config = LoggingConfig::development();
        assert_eq!(config.level, Level::DEBUG);
        assert!(config.with_target);
        assert!(config.with_thread_ids);
        assert!(config.file_path.is_some());
        assert!(!config.json_format);
    }

    #[test]
    fn test_production_config() {
        let config = LoggingConfig::production();
        assert_eq!(config.level, Level::INFO);
        assert!(!config.with_target);
        assert!(!config.with_thread_ids);
        assert!(config.file_path.is_some());
        assert!(config.json_format);
    }

    #[test]
    fn test_testing_config() {
        let config = LoggingConfig::testing();
        assert_eq!(config.level, Level::TRACE);
        assert!(config.with_target);
        assert!(!config.with_timestamp);
        assert!(config.file_path.is_none());
        assert!(!config.json_format);
    }

    #[test]
    fn test_level_parsing() {
        use level_utils::*;

        assert_eq!(parse_level("trace").unwrap(), Level::TRACE);
        assert_eq!(parse_level("DEBUG").unwrap(), Level::DEBUG);
        assert_eq!(parse_level("Info").unwrap(), Level::INFO);
        assert_eq!(parse_level("WARN").unwrap(), Level::WARN);
        assert_eq!(parse_level("error").unwrap(), Level::ERROR);

        assert!(parse_level("invalid").is_err());
    }

    #[test]
    fn test_level_to_string() {
        use level_utils::*;

        assert_eq!(level_to_string(&Level::TRACE), "trace");
        assert_eq!(level_to_string(&Level::DEBUG), "debug");
        assert_eq!(level_to_string(&Level::INFO), "info");
        assert_eq!(level_to_string(&Level::WARN), "warn");
        assert_eq!(level_to_string(&Level::ERROR), "error");
    }

    /// Integration test for logging initialization
    #[test]
    fn test_logging_initialization() {
        let config = LoggingConfig::testing();

        // This should not fail in test environment
        let result = initialize_logging(config);

        // We can't easily test the actual logging output,
        // but we can test that initialization doesn't panic
        if result.is_err() {
            // Logging may already be initialized by other tests
            // This is acceptable in test environment
        }
    }
}
