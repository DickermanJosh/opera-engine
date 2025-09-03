// Async integration tests for Opera UCI Engine
//
// These tests verify async operation coordination and timing behavior
// across the entire UCI engine system.

use opera_uci::testing::{MockUCICommands, PerformanceMeasurer, TestConfig, TestRuntime};
use opera_uci::{UCIError, UCIResult};
use std::time::Duration;
use tokio::time::timeout;

/// Test basic async runtime functionality
#[tokio::test]
async fn test_basic_async_runtime() -> UCIResult<()> {
    let runtime = TestRuntime::new(TestConfig::default());

    runtime
        .run_test("basic_async_op", async {
            tokio::time::sleep(Duration::from_millis(10)).await;
            Ok(())
        })
        .await?;

    Ok(())
}

/// Test timeout handling in async operations
#[tokio::test]
async fn test_timeout_handling() -> UCIResult<()> {
    let config = TestConfig {
        default_timeout: Duration::from_millis(50),
        ..Default::default()
    };
    let runtime = TestRuntime::new(config);

    // This should timeout
    let result = runtime
        .run_test("timeout_test", async {
            tokio::time::sleep(Duration::from_millis(100)).await;
            Ok(())
        })
        .await;

    match result {
        Err(UCIError::Timeout { .. }) => Ok(()),
        Ok(_) => Err(UCIError::Internal {
            message: "Expected timeout error".to_string(),
        }),
        Err(other) => Err(UCIError::Internal {
            message: format!("Expected timeout, got: {}", other),
        }),
    }
}

/// Test concurrent async operations
#[tokio::test]
async fn test_concurrent_operations() -> UCIResult<()> {
    let runtime = TestRuntime::new(TestConfig::default());

    runtime
        .run_test("concurrent_test", async {
            let tasks = vec![
                tokio::spawn(async { tokio::time::sleep(Duration::from_millis(10)).await }),
                tokio::spawn(async { tokio::time::sleep(Duration::from_millis(15)).await }),
                tokio::spawn(async { tokio::time::sleep(Duration::from_millis(20)).await }),
            ];

            // Wait for all tasks to complete
            for task in tasks {
                task.await.map_err(|e| UCIError::Internal {
                    message: format!("Task join error: {}", e),
                })?;
            }

            Ok(())
        })
        .await
}

/// Test performance measurement utilities
#[tokio::test]
async fn test_performance_measurement() -> UCIResult<()> {
    let runtime = TestRuntime::new(TestConfig::default());

    runtime
        .run_test("performance_test", async {
            let mut measurer = PerformanceMeasurer::new();

            // Measure several operations
            let _result1 = measurer
                .measure("op1", async {
                    tokio::time::sleep(Duration::from_millis(5)).await;
                    42
                })
                .await;

            let _result2 = measurer
                .measure("op2", async {
                    tokio::time::sleep(Duration::from_millis(10)).await;
                    "test"
                })
                .await;

            let _result3 = measurer
                .measure("op1", async {
                    tokio::time::sleep(Duration::from_millis(7)).await;
                    24
                })
                .await;

            // Verify measurements exist
            assert!(measurer.get_measurement("op1").is_some());
            assert!(measurer.get_measurement("op2").is_some());

            // Test average calculation for repeated operation
            let avg = measurer.average_duration("op1");
            assert!(avg.is_some());
            let avg_duration = avg.unwrap();
            assert!(avg_duration >= Duration::from_millis(5));

            Ok(())
        })
        .await
}

/// Test mock UCI command generation
#[tokio::test]
async fn test_mock_uci_commands() -> UCIResult<()> {
    let runtime = TestRuntime::new(TestConfig::default());

    runtime
        .run_test("mock_commands_test", async {
            let mut mock = MockUCICommands::new()
                .with_uci_init()
                .with_position("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")
                .with_go(1000);

            // Test command sequence
            let expected_commands = [
                "uci",
                "isready",
                "ucinewgame",
                "position fen rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
                "go movetime 1000",
            ];

            for expected in &expected_commands {
                let command = mock.next_command().ok_or_else(|| UCIError::Internal {
                    message: "Expected more commands in sequence".to_string(),
                })?;

                if command != *expected {
                    return Err(UCIError::Internal {
                        message: format!("Expected '{}', got '{}'", expected, command),
                    });
                }
            }

            // Should be no more commands
            assert!(mock.next_command().is_none());

            // Test reset functionality
            mock.reset();
            assert_eq!(mock.next_command(), Some("uci"));

            Ok(())
        })
        .await
}

/// Test error propagation in async chains
#[tokio::test]
async fn test_async_error_propagation() -> UCIResult<()> {
    let runtime = TestRuntime::new(TestConfig::default());

    // This should propagate the error correctly
    let result = runtime
        .run_test("error_propagation", async {
            async_operation_that_fails().await
        })
        .await;

    match result {
        Err(UCIError::Protocol { message }) if message.contains("test error") => Ok(()),
        Ok(_) => Err(UCIError::Internal {
            message: "Expected error propagation".to_string(),
        }),
        Err(other) => Err(UCIError::Internal {
            message: format!("Unexpected error type: {}", other),
        }),
    }
}

/// Helper function that simulates an async operation that fails
async fn async_operation_that_fails() -> UCIResult<()> {
    tokio::time::sleep(Duration::from_millis(1)).await;
    Err(UCIError::Protocol {
        message: "test error".to_string(),
    })
}

/// Test channel-based async communication
#[tokio::test]
async fn test_async_channel_communication() -> UCIResult<()> {
    let runtime = TestRuntime::new(TestConfig::default());

    runtime
        .run_test("channel_communication", async {
            let (tx, mut rx) = tokio::sync::mpsc::channel::<String>(10);

            // Spawn producer task
            let producer = tokio::spawn(async move {
                for i in 0..5 {
                    let message = format!("message_{}", i);
                    if tx.send(message).await.is_err() {
                        break;
                    }
                    tokio::time::sleep(Duration::from_millis(1)).await;
                }
            });

            // Spawn consumer task
            let consumer = tokio::spawn(async move {
                let mut messages = Vec::new();
                while let Some(message) = rx.recv().await {
                    messages.push(message);
                    if messages.len() >= 5 {
                        break;
                    }
                }
                messages
            });

            // Wait for both tasks and verify results
            let _ = producer.await.map_err(|e| UCIError::Internal {
                message: format!("Producer task error: {}", e),
            })?;

            let messages = consumer.await.map_err(|e| UCIError::Internal {
                message: format!("Consumer task error: {}", e),
            })?;

            if messages.len() != 5 {
                return Err(UCIError::Internal {
                    message: format!("Expected 5 messages, got {}", messages.len()),
                });
            }

            for (i, message) in messages.iter().enumerate() {
                let expected = format!("message_{}", i);
                if message != &expected {
                    return Err(UCIError::Internal {
                        message: format!("Expected '{}', got '{}'", expected, message),
                    });
                }
            }

            Ok(())
        })
        .await
}

/// Benchmark test for async operation performance
#[tokio::test]
async fn benchmark_async_operations() -> UCIResult<()> {
    let config = TestConfig {
        enable_benchmarks: true,
        default_timeout: Duration::from_secs(5),
        ..Default::default()
    };

    let runtime = TestRuntime::new(config);
    let mut measurer = PerformanceMeasurer::new();

    runtime
        .run_test("async_benchmark", async {
            // Benchmark different async operations
            let _result = measurer
                .measure("sleep_10ms", async {
                    tokio::time::sleep(Duration::from_millis(10)).await;
                })
                .await;

            let _result = measurer
                .measure("concurrent_sleeps", async {
                    let tasks = vec![
                        tokio::spawn(async { tokio::time::sleep(Duration::from_millis(5)).await }),
                        tokio::spawn(async { tokio::time::sleep(Duration::from_millis(5)).await }),
                        tokio::spawn(async { tokio::time::sleep(Duration::from_millis(5)).await }),
                    ];

                    for task in tasks {
                        task.await.unwrap();
                    }
                })
                .await;

            let _result = measurer
                .measure("channel_throughput", async {
                    let (tx, mut rx) = tokio::sync::mpsc::channel(100);

                    let producer = tokio::spawn(async move {
                        for i in 0..100 {
                            let _ = tx.send(i).await;
                        }
                    });

                    let consumer = tokio::spawn(async move {
                        let mut count = 0;
                        while let Some(_) = rx.recv().await {
                            count += 1;
                            if count >= 100 {
                                break;
                            }
                        }
                    });

                    let _ = tokio::try_join!(producer, consumer);
                })
                .await;

            // Print results
            measurer.print_summary();

            // Verify timing constraints
            let sleep_time =
                measurer
                    .get_measurement("sleep_10ms")
                    .ok_or_else(|| UCIError::Internal {
                        message: "Missing sleep measurement".to_string(),
                    })?;

            // Should take at least 10ms
            if sleep_time < Duration::from_millis(9) {
                return Err(UCIError::Internal {
                    message: format!("Sleep took {:?}, expected at least 9ms", sleep_time),
                });
            }

            // Concurrent operations should be faster than sequential
            let concurrent_time =
                measurer
                    .get_measurement("concurrent_sleeps")
                    .ok_or_else(|| UCIError::Internal {
                        message: "Missing concurrent measurement".to_string(),
                    })?;

            // Should be significantly less than 15ms (3 * 5ms sequential)
            if concurrent_time > Duration::from_millis(12) {
                return Err(UCIError::Internal {
                    message: format!("Concurrent operations too slow: {:?}", concurrent_time),
                });
            }

            Ok(())
        })
        .await
}
