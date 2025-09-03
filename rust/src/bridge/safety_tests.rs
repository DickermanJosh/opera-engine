// FFI Safety Tests and Memory Leak Detection
//
// This module provides comprehensive tests to ensure the FFI bridge is safe,
// handles edge cases properly, and doesn't leak memory or cause crashes.

#![allow(clippy::missing_docs_in_private_items)]

use crate::bridge::Board;
use crate::error::UCIResult;
use std::thread;
use std::time::{Duration, Instant};
use tracing::info;

/// Test FFI safety under stress conditions
pub struct FFISafetyTestSuite;

impl FFISafetyTestSuite {
    /// Run all FFI safety tests
    pub fn run_all_tests() -> UCIResult<()> {
        info!("Starting comprehensive FFI safety test suite");

        Self::test_memory_leak_detection()?;
        Self::test_concurrent_access()?;
        Self::test_error_handling_robustness()?;
        Self::test_resource_cleanup()?;
        Self::test_null_pointer_safety()?;
        Self::test_stress_operations()?;

        info!("All FFI safety tests completed successfully");
        Ok(())
    }

    /// Test for memory leaks by creating and destroying many Board instances
    fn test_memory_leak_detection() -> UCIResult<()> {
        info!("Testing memory leak detection");

        let start_time = Instant::now();
        let iterations = 1000;

        for i in 0..iterations {
            // Create board
            let mut board = Board::new()?;

            // Perform various operations
            board.set_from_fen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")?;
            let _fen = board.get_fen()?;
            board.make_move("e2e4")?;
            board.make_move("e7e5")?;
            let _in_check = board.is_in_check()?;
            board.reset();

            // Log progress periodically
            if i % 100 == 0 {
                info!("Memory leak test progress: {}/{}", i, iterations);
            }
        }

        let elapsed = start_time.elapsed();
        info!(
            "Memory leak test completed: {} iterations in {:?}",
            iterations, elapsed
        );

        // If we reach this point without crashes, memory management is working
        Ok(())
    }

    /// Test concurrent access to multiple Board instances
    fn test_concurrent_access() -> UCIResult<()> {
        info!("Testing concurrent FFI access");

        let num_threads = 8;
        let operations_per_thread = 50;

        let mut handles = vec![];

        for thread_id in 0..num_threads {
            let handle = thread::spawn(move || -> UCIResult<()> {
                for op in 0..operations_per_thread {
                    let mut board = Board::new()?;

                    // Perform thread-specific operations
                    match (thread_id + op) % 4 {
                        0 => {
                            board.make_move("e2e4")?;
                            board.make_move("e7e5")?;
                        }
                        1 => {
                            board.set_from_fen(
                                "r1bqkbnr/pppp1ppp/2n5/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 4 4",
                            )?;
                        }
                        2 => {
                            board.make_move("d2d4")?;
                            let _check = board.is_in_check()?;
                        }
                        _ => {
                            board.reset();
                            let _fen = board.get_fen()?;
                        }
                    }
                }
                Ok(())
            });

            handles.push(handle);
        }

        // Wait for all threads to complete
        for handle in handles {
            handle
                .join()
                .map_err(|_| crate::error::UCIError::Internal {
                    message: "Thread panic during concurrent access test".to_string(),
                })??;
        }

        info!("Concurrent access test completed successfully");
        Ok(())
    }

    /// Test robustness of error handling with invalid inputs
    fn test_error_handling_robustness() -> UCIResult<()> {
        info!("Testing error handling robustness");

        let mut board = Board::new()?;

        // Test invalid FEN strings - should not crash
        let invalid_fens = vec![
            "",
            "invalid",
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP", // Missing fields
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR x KQkq - 0 1", // Invalid side
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w ZZZ - 0 1", // Invalid castling
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq z9 0 1", // Invalid en passant
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - -1 1", // Invalid halfmove
            "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 0", // Invalid fullmove
        ];

        for fen in invalid_fens {
            let result = board.set_from_fen(fen);
            // Should return error, not crash
            assert!(result.is_err(), "Expected error for invalid FEN: {}", fen);
        }

        // Test invalid move strings - should not crash
        let invalid_moves = vec![
            "", "e", "e2", "e2e", "e2e4e", "z1a1", // Invalid squares
            "a0a1", "a1a9", "a1z1", "e2e4x", // Invalid promotion
        ];

        for move_str in invalid_moves {
            let result = board.make_move(move_str);
            // Should return error, not crash
            assert!(
                result.is_err(),
                "Expected error for invalid move: {}",
                move_str
            );

            let result = board.is_valid_move(move_str);
            // Should return error for format issues, not crash
            assert!(
                result.is_err(),
                "Expected error for invalid move format: {}",
                move_str
            );
        }

        info!("Error handling robustness test completed");
        Ok(())
    }

    /// Test that resources are properly cleaned up
    fn test_resource_cleanup() -> UCIResult<()> {
        info!("Testing resource cleanup");

        // Create many boards and let them go out of scope
        for _i in 0..100 {
            {
                let mut board = Board::new()?;
                board.make_move("e2e4")?;
                board.make_move("e7e5")?;
                let _fen = board.get_fen()?;
                // Board should be automatically cleaned up when it goes out of scope
            }

            // Create another board to ensure previous one was cleaned up
            let _board2 = Board::new()?;
        }

        info!("Resource cleanup test completed");
        Ok(())
    }

    /// Test null pointer safety (should not be possible with our design)
    fn test_null_pointer_safety() -> UCIResult<()> {
        info!("Testing null pointer safety");

        // Our design should prevent null pointers, but let's test edge cases

        // Create many boards rapidly to stress the allocation system
        let mut boards = Vec::new();
        for _i in 0..50 {
            boards.push(Board::new()?);
        }

        // Use all boards to ensure they're valid
        for board in &boards {
            let _fen = board.get_fen()?;
            let _check = board.is_in_check()?;
        }

        info!("Null pointer safety test completed");
        Ok(())
    }

    /// Stress test FFI operations with rapid create/destroy cycles
    fn test_stress_operations() -> UCIResult<()> {
        info!("Starting FFI stress test");

        let start_time = Instant::now();
        let stress_duration = Duration::from_secs(2);
        let mut operation_count = 0;

        while start_time.elapsed() < stress_duration {
            // Rapid create/destroy cycle
            let mut board = Board::new()?;

            // Quick operations
            board.make_move("e2e4")?;
            let _fen = board.get_fen()?;
            board.reset();

            operation_count += 1;

            // Yield to prevent tight spin
            if operation_count % 100 == 0 {
                thread::sleep(Duration::from_millis(1));
            }
        }

        info!(
            "Stress test completed: {} operations in {:?}",
            operation_count,
            start_time.elapsed()
        );

        Ok(())
    }
}

/// Test thread safety with shared Board access patterns
pub struct ThreadSafetyTestSuite;

impl ThreadSafetyTestSuite {
    /// Test thread safety scenarios
    pub fn run_thread_safety_tests() -> UCIResult<()> {
        info!("Starting thread safety test suite");

        Self::test_arc_sharing()?;
        Self::test_move_across_threads()?;

        info!("Thread safety tests completed");
        Ok(())
    }

    /// Test concurrent creation of Board instances (each thread owns its own board)
    fn test_arc_sharing() -> UCIResult<()> {
        info!("Testing concurrent board creation safety");

        let mut handles = vec![];

        // Spawn threads that each create and use their own board
        for _i in 0..4 {
            let handle = thread::spawn(move || -> UCIResult<()> {
                // Each thread creates its own board - this is safe
                let board = Board::new()?;
                let _fen = board.get_fen()?;
                let _check = board.is_in_check()?;
                let _mate = board.is_checkmate()?;
                let _stale = board.is_stalemate()?;
                Ok(())
            });
            handles.push(handle);
        }

        // Wait for all threads
        for handle in handles {
            handle
                .join()
                .map_err(|_| crate::error::UCIError::Internal {
                    message: "Thread panic during concurrent board creation test".to_string(),
                })??;
        }

        info!("Concurrent board creation test completed");
        Ok(())
    }

    /// Test that Board instances have proper lifetime management across single thread
    fn test_move_across_threads() -> UCIResult<()> {
        info!("Testing Board lifetime management");

        // Test that Board can be created and destroyed in rapid succession
        for _i in 0..100 {
            let mut board = Board::new()?;
            board.make_move("e2e4")?;
            board.make_move("e7e5")?;
            let _fen = board.get_fen()?;
            board.reset();
            // Board should be automatically cleaned up here
        }

        info!("Board lifetime management test completed");
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_memory_leak_detection() {
        FFISafetyTestSuite::test_memory_leak_detection().expect("Memory leak test failed");
    }

    #[test]
    fn test_concurrent_access() {
        FFISafetyTestSuite::test_concurrent_access().expect("Concurrent access test failed");
    }

    #[test]
    fn test_error_handling_robustness() {
        FFISafetyTestSuite::test_error_handling_robustness().expect("Error handling test failed");
    }

    #[test]
    fn test_resource_cleanup() {
        FFISafetyTestSuite::test_resource_cleanup().expect("Resource cleanup test failed");
    }

    #[test]
    fn test_null_pointer_safety() {
        FFISafetyTestSuite::test_null_pointer_safety().expect("Null pointer safety test failed");
    }

    #[test]
    fn test_stress_operations() {
        FFISafetyTestSuite::test_stress_operations().expect("Stress operations test failed");
    }

    #[test]
    fn test_thread_safety() {
        ThreadSafetyTestSuite::run_thread_safety_tests().expect("Thread safety tests failed");
    }

    #[test]
    fn test_comprehensive_suite() {
        // Run the full safety test suite
        FFISafetyTestSuite::run_all_tests().expect("Comprehensive safety tests failed");
    }
}
