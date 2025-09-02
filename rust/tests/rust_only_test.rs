// Rust-only tests for async runtime and logging functionality
// These tests verify the core Rust infrastructure without C++ FFI

#[cfg(test)]
mod tests {
    use std::time::Duration;

    #[tokio::test]
    async fn test_tokio_runtime() {
        // Test basic tokio functionality
        let start = tokio::time::Instant::now();
        tokio::time::sleep(Duration::from_millis(10)).await;
        let duration = start.elapsed();
        assert!(duration >= Duration::from_millis(10));
    }

    #[tokio::test]
    async fn test_concurrent_tasks() {
        // Test concurrent task execution
        let tasks = vec![
            tokio::spawn(async { 42 }),
            tokio::spawn(async { 24 }),
            tokio::spawn(async { 99 }),
        ];

        let mut results = Vec::new();
        for task in tasks {
            results.push(task.await.unwrap());
        }

        assert_eq!(results.len(), 3);
        assert!(results.contains(&42));
        assert!(results.contains(&24));
        assert!(results.contains(&99));
    }

    #[tokio::test]
    async fn test_timeout_handling() {
        // Test timeout functionality
        let result = tokio::time::timeout(
            Duration::from_millis(50),
            tokio::time::sleep(Duration::from_millis(100))
        ).await;

        assert!(result.is_err()); // Should timeout

        let result = tokio::time::timeout(
            Duration::from_millis(100),
            tokio::time::sleep(Duration::from_millis(10))
        ).await;

        assert!(result.is_ok()); // Should succeed
    }

    #[test]
    fn test_error_types() {
        use opera_uci::error::UCIError;

        let error = UCIError::Protocol {
            message: "Test error".to_string(),
        };

        let display = format!("{}", error);
        assert!(display.contains("UCI protocol error"));
    }

    #[test] 
    fn test_never_panic_utilities() {
        use opera_uci::error::never_panic;

        // Test safe parsing
        let result: Result<i32, _> = never_panic::safe_parse("42", "test");
        assert!(result.is_ok());
        assert_eq!(result.unwrap(), 42);

        let result: Result<i32, _> = never_panic::safe_parse("invalid", "test");
        assert!(result.is_err());

        // Test safe division
        let result = never_panic::safe_divide(10, 2, "test");
        assert!(result.is_ok());
        assert_eq!(result.unwrap(), 5);

        let result = never_panic::safe_divide(10, 0, "test");
        assert!(result.is_err());
    }

    #[test]
    fn test_logging_config_creation() {
        use opera_uci::logging::LoggingConfig;

        // Test that logging configs can be created
        let _default = LoggingConfig::default();
        let _dev = LoggingConfig::development();
        let _prod = LoggingConfig::production();
        let _test = LoggingConfig::testing();
        let _uci = LoggingConfig::uci_debug();

        // Verify some basic properties
        let dev = LoggingConfig::development();
        assert!(dev.with_target);
        assert_eq!(dev.level, tracing::Level::DEBUG);
    }

    #[test]
    fn test_runtime_config_creation() {
        use opera_uci::runtime::RuntimeConfig;

        // Test that runtime configs can be created
        let _default = RuntimeConfig::default();
        let _dev = RuntimeConfig::development();
        let _prod = RuntimeConfig::performance();
        let _uci = RuntimeConfig::uci_optimized();

        // Verify some basic properties
        let dev = RuntimeConfig::development();
        assert!(dev.enable_io);
        assert!(dev.enable_time);
    }

    #[test]
    fn test_error_context() {
        use opera_uci::error::{ErrorContext, UCIError, ResultExt};

        let context = ErrorContext::new("test operation")
            .detail("test detail 1")
            .detail("test detail 2");

        assert_eq!(context.operation, "test operation");
        assert_eq!(context.details.len(), 2);

        // Test ResultExt trait
        let result: Result<(), UCIError> = Err(UCIError::Protocol {
            message: "test".to_string(),
        });

        let contextual_result = result.with_operation("test op");
        assert!(contextual_result.is_err());
    }
}