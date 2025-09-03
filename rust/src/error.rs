// Error handling system for Opera UCI Engine
//
// This module provides comprehensive error types following the never-panic principle.
// All error conditions are handled gracefully with proper recovery mechanisms.

#![allow(unsafe_code)] // Required for error handling in FFI contexts

use std::fmt;
use thiserror::Error;

/// Main error type for UCI operations
///
/// This enum covers all possible error conditions in the UCI engine,
/// ensuring exhaustive error handling without panics.
#[derive(Error, Debug, Clone, PartialEq)]
pub enum UCIError {
    /// Errors related to UCI protocol parsing and validation
    #[error("UCI protocol error: {message}")]
    Protocol { message: String },

    /// Errors from the C++ engine core via FFI
    #[error("Engine error: {message}")]
    Engine { message: String },

    /// Board position and FEN string errors
    #[error("Board position error: {message}")]
    Position { message: String },

    /// Move parsing and validation errors
    #[error("Move error: {message}")]
    Move { message: String },

    /// Search and time management errors
    #[error("Search error: {message}")]
    Search { message: String },

    /// Configuration and option setting errors
    #[error("Configuration error: {message}")]
    Configuration { message: String },

    /// I/O and communication errors
    #[error("I/O error: {message}")]
    Io { message: String },

    /// FFI boundary errors (C++ integration)
    #[error("FFI error: {message}")]
    Ffi { message: String },

    /// Timeout errors for search and operations
    #[error("Timeout error: operation timed out after {duration_ms}ms")]
    Timeout { duration_ms: u64 },

    /// Resource exhaustion errors
    #[error("Resource error: {resource} exhausted")]
    Resource { resource: String },

    /// Internal state consistency errors
    #[error("Internal error: {message} (this should not happen)")]
    Internal { message: String },
}

/// Result type alias for UCI operations
pub type UCIResult<T> = Result<T, UCIError>;

/// Convert std::io::Error to UCIError
impl From<std::io::Error> for UCIError {
    fn from(error: std::io::Error) -> Self {
        UCIError::Io {
            message: error.to_string(),
        }
    }
}

/// Error context for providing additional debugging information
#[derive(Debug, Clone)]
pub struct ErrorContext {
    pub operation: String,
    pub details: Vec<String>,
    pub timestamp: std::time::SystemTime,
}

impl ErrorContext {
    /// Create new error context
    pub fn new(operation: impl Into<String>) -> Self {
        Self {
            operation: operation.into(),
            details: Vec::new(),
            timestamp: std::time::SystemTime::now(),
        }
    }

    /// Add contextual detail
    pub fn detail(mut self, detail: impl Into<String>) -> Self {
        self.details.push(detail.into());
        self
    }
}

/// Enhanced error with context information
#[derive(Debug)]
pub struct ContextualError {
    pub error: UCIError,
    pub context: ErrorContext,
}

impl fmt::Display for ContextualError {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{} (in {})", self.error, self.context.operation)?;
        if !self.context.details.is_empty() {
            write!(f, " - Details: {}", self.context.details.join(", "))?;
        }
        Ok(())
    }
}

impl std::error::Error for ContextualError {}

/// Result type with context
pub type ContextualResult<T> = Result<T, ContextualError>;

/// Extension trait for adding context to results
pub trait ResultExt<T> {
    /// Add context to an error result
    fn with_context(self, context: ErrorContext) -> ContextualResult<T>;

    /// Add simple context with operation name
    fn with_operation(self, operation: impl Into<String>) -> ContextualResult<T>;
}

impl<T> ResultExt<T> for UCIResult<T> {
    fn with_context(self, context: ErrorContext) -> ContextualResult<T> {
        self.map_err(|error| ContextualError { error, context })
    }

    fn with_operation(self, operation: impl Into<String>) -> ContextualResult<T> {
        let context = ErrorContext::new(operation);
        self.with_context(context)
    }
}

/// Never-panic utilities for safe operations
pub mod never_panic {
    use super::*;

    /// Safe string parsing that never panics
    pub fn safe_parse<T: std::str::FromStr>(input: &str, operation: &str) -> UCIResult<T> {
        input.parse().map_err(|_| UCIError::Protocol {
            message: format!("Failed to parse {} from '{}'", operation, input),
        })
    }

    /// Safe array/vector access that never panics
    pub fn safe_get<T: Clone>(collection: &[T], index: usize, context: &str) -> UCIResult<T> {
        collection
            .get(index)
            .cloned()
            .ok_or_else(move || UCIError::Internal {
                message: format!(
                    "Index {} out of bounds in {} (length: {})",
                    index,
                    context,
                    collection.len()
                ),
            })
    }

    /// Safe string slicing that never panics
    pub fn safe_slice(
        input: &str,
        start: usize,
        end: Option<usize>,
        context: &str,
    ) -> UCIResult<String> {
        let len = input.len();
        if start > len {
            return Err(UCIError::Protocol {
                message: format!(
                    "Start index {} exceeds string length {} in {}",
                    start, len, context
                ),
            });
        }

        let end = end.unwrap_or(len);
        if end > len {
            return Err(UCIError::Protocol {
                message: format!(
                    "End index {} exceeds string length {} in {}",
                    end, len, context
                ),
            });
        }

        if start > end {
            return Err(UCIError::Protocol {
                message: format!("Start index {} > end index {} in {}", start, end, context),
            });
        }

        Ok(input[start..end].to_string())
    }

    /// Safe division that handles zero denominators
    pub fn safe_divide(numerator: i64, denominator: i64, context: &str) -> UCIResult<i64> {
        if denominator == 0 {
            return Err(UCIError::Internal {
                message: format!("Division by zero in {}", context),
            });
        }
        Ok(numerator / denominator)
    }
}

/// Error recovery strategies
pub mod recovery {
    use super::*;
    use tracing::{debug, error, warn};

    /// Recovery strategy for different error types
    pub enum RecoveryAction {
        /// Continue operation with default value
        ContinueWithDefault,
        /// Retry operation once
        RetryOnce,
        /// Skip current operation and continue
        Skip,
        /// Reset to safe state and continue
        ResetState,
        /// Terminate gracefully
        Terminate,
    }

    /// Determine recovery strategy for an error
    pub fn recovery_strategy(error: &UCIError) -> RecoveryAction {
        match error {
            // Protocol errors: try to continue with safe defaults
            UCIError::Protocol { .. } => RecoveryAction::ContinueWithDefault,

            // Engine errors: reset state and try to continue
            UCIError::Engine { .. } => RecoveryAction::ResetState,

            // Position errors: skip invalid position and continue
            UCIError::Position { .. } => RecoveryAction::Skip,

            // Move errors: ignore invalid move and continue
            UCIError::Move { .. } => RecoveryAction::Skip,

            // Search errors: reset search state
            UCIError::Search { .. } => RecoveryAction::ResetState,

            // Configuration errors: use defaults
            UCIError::Configuration { .. } => RecoveryAction::ContinueWithDefault,

            // I/O errors: retry once, then terminate
            UCIError::Io { .. } => RecoveryAction::RetryOnce,

            // FFI errors: reset state (may be serious)
            UCIError::Ffi { .. } => RecoveryAction::ResetState,

            // Timeout errors: continue (search can timeout)
            UCIError::Timeout { .. } => RecoveryAction::ContinueWithDefault,

            // Resource errors: retry once
            UCIError::Resource { .. } => RecoveryAction::RetryOnce,

            // Internal errors: log and try to continue
            UCIError::Internal { .. } => RecoveryAction::ContinueWithDefault,
        }
    }

    /// Execute error recovery with logging
    pub fn recover_from_error(error: &UCIError, context: &str) {
        let strategy = recovery_strategy(error);

        match strategy {
            RecoveryAction::ContinueWithDefault => {
                warn!("Error in {}: {} - continuing with default", context, error);
            }
            RecoveryAction::RetryOnce => {
                debug!("Error in {}: {} - will retry once", context, error);
            }
            RecoveryAction::Skip => {
                debug!("Error in {}: {} - skipping operation", context, error);
            }
            RecoveryAction::ResetState => {
                warn!("Error in {}: {} - resetting state", context, error);
            }
            RecoveryAction::Terminate => {
                error!("Fatal error in {}: {} - terminating", context, error);
            }
        }
    }
}

/// Macros for convenient error handling
#[macro_export]
macro_rules! uci_ensure {
    ($condition:expr, $error:expr) => {
        if !$condition {
            return Err($error);
        }
    };
}

#[macro_export]
macro_rules! uci_bail {
    ($error:expr) => {
        return Err($error);
    };
}

#[macro_export]
macro_rules! uci_context {
    ($result:expr, $operation:expr) => {
        $result.with_operation($operation)
    };
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_error_creation() {
        let error = UCIError::Protocol {
            message: "Invalid command".to_string(),
        };
        assert_eq!(error.to_string(), "UCI protocol error: Invalid command");
    }

    #[test]
    fn test_error_context() {
        let context = ErrorContext::new("parsing")
            .detail("command: go")
            .detail("line: 5");

        assert_eq!(context.operation, "parsing");
        assert_eq!(context.details.len(), 2);
    }

    #[test]
    fn test_never_panic_parse() {
        // Valid parse
        let result: UCIResult<i32> = never_panic::safe_parse("42", "number");
        assert!(result.is_ok());
        assert_eq!(result.unwrap(), 42);

        // Invalid parse
        let result: UCIResult<i32> = never_panic::safe_parse("invalid", "number");
        assert!(result.is_err());
    }

    #[test]
    fn test_safe_get() {
        let data = vec![1, 2, 3];

        // Valid access
        let result = never_panic::safe_get(&data, 1, "test");
        assert!(result.is_ok());
        assert_eq!(result.unwrap(), 2);

        // Invalid access
        let result = never_panic::safe_get(&data, 5, "test");
        assert!(result.is_err());
    }

    #[test]
    fn test_safe_slice() {
        let text = "hello world";

        // Valid slice
        let result = never_panic::safe_slice(text, 0, Some(5), "test");
        assert!(result.is_ok());
        assert_eq!(result.unwrap(), "hello".to_string());

        // Invalid slice (start > end)
        let result = never_panic::safe_slice(text, 5, Some(3), "test");
        assert!(result.is_err());

        // Invalid slice (out of bounds)
        let result = never_panic::safe_slice(text, 0, Some(20), "test");
        assert!(result.is_err());
    }

    #[test]
    fn test_safe_divide() {
        // Valid division
        let result = never_panic::safe_divide(10, 2, "test");
        assert!(result.is_ok());
        assert_eq!(result.unwrap(), 5);

        // Division by zero
        let result = never_panic::safe_divide(10, 0, "test");
        assert!(result.is_err());
    }

    #[test]
    fn test_recovery_strategies() {
        use recovery::*;

        let protocol_error = UCIError::Protocol {
            message: "test".to_string(),
        };
        let strategy = recovery_strategy(&protocol_error);
        assert!(matches!(strategy, RecoveryAction::ContinueWithDefault));

        let io_error = UCIError::Io {
            message: "Broken pipe error".to_string(),
        };
        let strategy = recovery_strategy(&io_error);
        assert!(matches!(strategy, RecoveryAction::RetryOnce));
    }

    #[test]
    fn test_result_extension() {
        let result: UCIResult<i32> = Ok(42);
        let contextual = result.with_operation("test operation");
        assert!(contextual.is_ok());

        let error_result: UCIResult<i32> = Err(UCIError::Protocol {
            message: "test error".to_string(),
        });
        let contextual = error_result.with_operation("test operation");
        assert!(contextual.is_err());

        let ctx_error = contextual.unwrap_err();
        assert_eq!(ctx_error.context.operation, "test operation");
    }
}
