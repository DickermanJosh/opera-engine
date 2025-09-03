// Basic functionality tests for async runtime and logging
//
// These tests verify that the core infrastructure works correctly
// before marking tasks as complete (following TDD principles)

use opera_uci::error::{UCIError, UCIResult};
use opera_uci::logging::{initialize_logging, LoggingConfig};
use opera_uci::runtime::{RuntimeConfig, RuntimeManager};
use std::time::Duration;

#[tokio::test]
async fn test_logging_initialization() -> UCIResult<()> {
    // Test that logging can be initialized with default config
    let config = LoggingConfig::default();

    // This should not panic or return error
    match initialize_logging(config) {
        Ok(()) => {
            // Verify we can log after initialization
            tracing::info!("Test log message - logging working correctly");
            Ok(())
        }
        Err(e) => {
            // If logging already initialized by another test, that's acceptable
            if e.to_string().contains("set_logger") || e.to_string().contains("already been set") {
                Ok(())
            } else {
                Err(e)
            }
        }
    }
}

#[tokio::test]
async fn test_logging_configs() -> UCIResult<()> {
    // Test different logging configurations don't panic
    let _dev_config = LoggingConfig::development();
    let _prod_config = LoggingConfig::production();
    let _test_config = LoggingConfig::testing();
    let _uci_config = LoggingConfig::uci_debug();

    // Just verify they can be created without panicking
    Ok(())
}

#[tokio::test]
async fn test_runtime_config_creation() -> UCIResult<()> {
    // Test that runtime configs can be created
    let _default_config = RuntimeConfig::default();
    let _dev_config = RuntimeConfig::development();
    let _prod_config = RuntimeConfig::production();
    let _uci_config = RuntimeConfig::uci_optimized();

    // Verify they have expected characteristics
    let dev_config = RuntimeConfig::development();
    assert!(dev_config.enable_io);
    assert!(dev_config.enable_time);
    assert_eq!(dev_config.thread_name, "opera-uci-dev");

    Ok(())
}

#[tokio::test]
async fn test_runtime_manager_creation() -> UCIResult<()> {
    // Test that runtime manager can be created
    let config = RuntimeConfig::default();
    let manager = RuntimeManager::new(config)?;

    // Verify basic operations work
    let runtime = manager.get_runtime();
    assert!(!runtime.id().is_null());

    Ok(())
}

#[tokio::test]
async fn test_async_operations() -> UCIResult<()> {
    // Test basic async operations work
    let start = tokio::time::Instant::now();

    // Simple async operation
    tokio::time::sleep(Duration::from_millis(10)).await;

    let duration = start.elapsed();
    assert!(duration >= Duration::from_millis(10));
    assert!(duration < Duration::from_millis(100)); // Should be quick

    Ok(())
}

#[tokio::test]
async fn test_concurrent_tasks() -> UCIResult<()> {
    // Test that we can run concurrent async tasks
    let tasks = vec![
        tokio::spawn(async {
            tokio::time::sleep(Duration::from_millis(5)).await;
            42
        }),
        tokio::spawn(async {
            tokio::time::sleep(Duration::from_millis(10)).await;
            24
        }),
        tokio::spawn(async {
            tokio::time::sleep(Duration::from_millis(15)).await;
            99
        }),
    ];

    // Wait for all tasks to complete
    let mut results = Vec::new();
    for task in tasks {
        let result = task.await.map_err(|e| UCIError::Internal {
            message: format!("Task join error: {}", e),
        })?;
        results.push(result);
    }

    // Verify results
    assert_eq!(results, vec![42, 24, 99]);

    Ok(())
}

#[tokio::test]
async fn test_error_handling() -> UCIResult<()> {
    // Test that our error types work correctly
    let protocol_error = UCIError::Protocol {
        message: "Test protocol error".to_string(),
    };

    let engine_error = UCIError::Engine {
        message: "Test engine error".to_string(),
    };

    // Verify error display works
    let protocol_display = format!("{}", protocol_error);
    let engine_display = format!("{}", engine_error);

    assert!(protocol_display.contains("UCI protocol error"));
    assert!(engine_display.contains("Engine error"));

    // Test error comparison
    let same_error = UCIError::Protocol {
        message: "Test protocol error".to_string(),
    };
    assert_eq!(protocol_error, same_error);

    Ok(())
}

#[tokio::test]
async fn test_timeout_handling() -> UCIResult<()> {
    // Test timeout functionality
    let result = tokio::time::timeout(
        Duration::from_millis(50),
        tokio::time::sleep(Duration::from_millis(100)),
    )
    .await;

    // Should timeout
    assert!(result.is_err());

    // Test successful operation within timeout
    let result = tokio::time::timeout(
        Duration::from_millis(100),
        tokio::time::sleep(Duration::from_millis(10)),
    )
    .await;

    // Should succeed
    assert!(result.is_ok());

    Ok(())
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

    // Test safe array access
    let data = vec![1, 2, 3];
    let result = never_panic::safe_get(&data, 1, "test");
    assert!(result.is_ok());
    assert_eq!(result.unwrap(), 2);

    let result = never_panic::safe_get(&data, 5, "test");
    assert!(result.is_err());

    // Test safe division
    let result = never_panic::safe_divide(10, 2, "test");
    assert!(result.is_ok());
    assert_eq!(result.unwrap(), 5);

    let result = never_panic::safe_divide(10, 0, "test");
    assert!(result.is_err());
}
