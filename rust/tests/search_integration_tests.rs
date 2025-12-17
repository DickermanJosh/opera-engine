// Integration tests for SearchEngine FFI bridge
//
// This module provides comprehensive integration tests for the search engine FFI,
// validating functionality, performance, thread safety, and async operation.

#![cfg(feature = "ffi")]

use opera_uci::bridge::{Board, SearchEngine, SearchLimits, SearchResult};
use opera_uci::error::UCIResult;
use std::thread;
use std::time::{Duration, Instant};

// ============================================================================
// Basic SearchEngine Tests
// ============================================================================

#[test]
fn test_search_engine_creation() {
    let mut board = Board::new().expect("Failed to create board");
    let engine = SearchEngine::new(&mut board);
    assert!(engine.is_ok(), "Failed to create search engine");
}

#[test]
fn test_search_depth_limited() {
    let mut board = Board::new().expect("Failed to create board");
    let mut engine = SearchEngine::new(&mut board).expect("Failed to create engine");

    let limits = SearchLimits::depth(5);
    let result = engine.search(limits);

    assert!(result.is_ok(), "Search failed: {:?}", result.err());
    let result = result.unwrap();
    assert!(result.is_valid(), "Search returned invalid result");
    assert!(!result.best_move.is_empty(), "Best move should not be empty");
    assert!(result.depth > 0, "Search depth should be > 0");
    assert!(result.nodes > 0, "Nodes searched should be > 0");
}

#[test]
fn test_search_starting_position() {
    let mut board = Board::new().expect("Failed to create board");
    let mut engine = SearchEngine::new(&mut board).expect("Failed to create engine");

    let limits = SearchLimits::depth(4);
    let result = engine.search(limits).expect("Search failed");

    assert!(result.is_valid(), "Invalid search result");
    assert_eq!(result.best_move.len(), 4, "Move should be 4 characters (e.g., e2e4)");

    // Verify it's a valid starting move
    let valid_starting_moves = ["e2e4", "d2d4", "g1f3", "b1c3", "c2c4", "f2f4"];
    assert!(
        valid_starting_moves.contains(&result.best_move.as_str()),
        "Best move '{}' should be a valid opening move",
        result.best_move
    );
}

#[test]
fn test_search_tactical_position() {
    let mut board = Board::new().expect("Failed to create board");

    // Position with tactical opportunity (Scholar's Mate setup)
    board
        .set_from_fen("r1bqkb1r/pppp1ppp/2n2n2/4p3/2B1P3/5Q2/PPPP1PPP/RNB1K1NR w KQkq - 0 1")
        .expect("Failed to set FEN");

    let mut engine = SearchEngine::new(&mut board).expect("Failed to create engine");
    let limits = SearchLimits::depth(3);
    let result = engine.search(limits).expect("Search failed");

    assert!(result.is_valid(), "Invalid search result");
    assert!(result.nodes > 100, "Should search at least 100 nodes");
}

#[test]
fn test_search_endgame_position() {
    let mut board = Board::new().expect("Failed to create board");

    // Simple endgame position
    board
        .set_from_fen("8/5k2/3p4/1p1Pp2p/pP2Pp1P/P4P1K/8/8 b - - 0 1")
        .expect("Failed to set FEN");

    let mut engine = SearchEngine::new(&mut board).expect("Failed to create engine");
    let limits = SearchLimits::depth(6); // Can search deeper in endgames
    let result = engine.search(limits).expect("Search failed");

    assert!(result.is_valid(), "Invalid search result");
}

// ============================================================================
// Search Limits Tests
// ============================================================================

#[test]
fn test_search_time_limited() {
    let mut board = Board::new().expect("Failed to create board");
    let mut engine = SearchEngine::new(&mut board).expect("Failed to create engine");

    let time_limit = Duration::from_millis(100);
    let limits = SearchLimits::time(time_limit);

    let start = Instant::now();
    let result = engine.search(limits).expect("Search failed");
    let elapsed = start.elapsed();

    assert!(result.is_valid(), "Invalid search result");
    // Should complete within reasonable margin (2x time limit)
    assert!(
        elapsed < time_limit * 2,
        "Search took too long: {:?} (limit: {:?})",
        elapsed,
        time_limit
    );
}

#[test]
fn test_search_node_limited() {
    let mut board = Board::new().expect("Failed to create board");
    let mut engine = SearchEngine::new(&mut board).expect("Failed to create engine");

    let node_limit = 10000;
    let limits = SearchLimits::nodes(node_limit);
    let result = engine.search(limits).expect("Search failed");

    assert!(result.is_valid(), "Invalid search result");
    // Should not exceed node limit by much (allow some overhead)
    assert!(
        result.nodes <= node_limit * 2,
        "Nodes {} exceeds limit {} by too much",
        result.nodes,
        node_limit
    );
}

#[test]
fn test_search_limits_default() {
    let limits = SearchLimits::default();
    assert_eq!(limits.max_depth, 64);
    assert_eq!(limits.max_nodes, u64::MAX);
    assert_eq!(limits.max_time_ms, u64::MAX);
    assert!(!limits.infinite);
}

// ============================================================================
// Search Stop and Control Tests
// ============================================================================

#[test]
fn test_search_stop() {
    let mut board = Board::new().expect("Failed to create board");
    let mut engine = SearchEngine::new(&mut board).expect("Failed to create engine");

    // Start a long search in background thread
    let engine_ptr = &mut engine as *mut SearchEngine;

    let handle = thread::spawn(move || {
        let engine = unsafe { &mut *engine_ptr };
        let limits = SearchLimits::depth(20); // Deep search
        engine.search(limits)
    });

    // Give search time to start
    thread::sleep(Duration::from_millis(50));

    // Stop the search
    engine.stop();

    // Wait for result
    let result = handle.join().expect("Thread panicked");

    // Should return a result (may be incomplete due to stop)
    assert!(result.is_ok(), "Search should complete after stop");
}

#[test]
fn test_search_is_searching_flag() {
    let mut board = Board::new().expect("Failed to create board");
    let mut engine = SearchEngine::new(&mut board).expect("Failed to create engine");

    // Initially not searching
    assert!(!engine.is_searching(), "Should not be searching initially");

    // After search completes, should not be searching
    let _result = engine.search(SearchLimits::depth(3)).expect("Search failed");
    assert!(!engine.is_searching(), "Should not be searching after completion");
}

#[test]
fn test_search_reset() {
    let mut board = Board::new().expect("Failed to create board");
    let mut engine = SearchEngine::new(&mut board).expect("Failed to create engine");

    // Perform first search
    let _result1 = engine.search(SearchLimits::depth(4)).expect("Search failed");
    assert!(engine.get_last_result().is_some(), "Should have last result");

    // Reset engine
    engine.reset();

    // Last result should be cleared
    assert!(engine.get_last_result().is_none(), "Last result should be cleared after reset");

    // Should still be able to search
    let result2 = engine.search(SearchLimits::depth(4));
    assert!(result2.is_ok(), "Should be able to search after reset");
}

// ============================================================================
// Search Result Tests
// ============================================================================

#[test]
fn test_search_result_contains_pv() {
    let mut board = Board::new().expect("Failed to create board");
    let mut engine = SearchEngine::new(&mut board).expect("Failed to create engine");

    let result = engine.search(SearchLimits::depth(5)).expect("Search failed");

    assert!(result.is_valid(), "Invalid result");
    assert!(!result.principal_variation.is_empty(), "PV should not be empty");
    assert_eq!(
        result.principal_variation[0], result.best_move,
        "First PV move should match best move"
    );
}

#[test]
fn test_search_result_statistics() {
    let mut board = Board::new().expect("Failed to create board");
    let mut engine = SearchEngine::new(&mut board).expect("Failed to create engine");

    let result = engine.search(SearchLimits::depth(5)).expect("Search failed");

    assert!(result.is_valid(), "Invalid result");
    assert!(result.nodes > 0, "Should have searched some nodes");
    assert!(result.time_ms > 0, "Should have taken some time");
    assert!(result.depth > 0, "Should have reached some depth");

    // NPS should be reasonable (at least 1K nps)
    let nps = result.nps();
    assert!(nps > 1000, "NPS should be > 1000, got {}", nps);
}

#[test]
fn test_search_result_display() {
    let result = SearchResult {
        best_move: "e2e4".to_string(),
        ponder_move: "e7e5".to_string(),
        score: 50,
        depth: 10,
        nodes: 100000,
        time_ms: 1000,
        principal_variation: vec!["e2e4".to_string(), "e7e5".to_string()],
    };

    let display = format!("{}", result);
    assert!(display.contains("bestmove e2e4"), "Display should contain bestmove");
    assert!(display.contains("ponder e7e5"), "Display should contain ponder");
    assert!(display.contains("score 50"), "Display should contain score");
}

// ============================================================================
// Performance Tests
// ============================================================================

#[test]
fn test_search_performance_minimum_nps() {
    let mut board = Board::new().expect("Failed to create board");
    let mut engine = SearchEngine::new(&mut board).expect("Failed to create engine");

    let result = engine.search(SearchLimits::depth(5)).expect("Search failed");

    let nps = result.nps();
    // Should achieve at least 10K nps (very conservative for basic test)
    assert!(nps > 10000, "NPS too low: {} (expected > 10K)", nps);
}

#[test]
fn test_search_scaling_with_depth() {
    let mut board = Board::new().expect("Failed to create board");
    let mut engine = SearchEngine::new(&mut board).expect("Failed to create engine");

    let depth3 = engine.search(SearchLimits::depth(3)).expect("Search failed");
    engine.reset();

    let depth4 = engine.search(SearchLimits::depth(4)).expect("Search failed");
    engine.reset();

    let depth5 = engine.search(SearchLimits::depth(5)).expect("Search failed");

    // Nodes should generally increase with depth
    assert!(
        depth4.nodes > depth3.nodes,
        "Depth 4 nodes ({}) should be > depth 3 nodes ({})",
        depth4.nodes,
        depth3.nodes
    );
    assert!(
        depth5.nodes > depth4.nodes,
        "Depth 5 nodes ({}) should be > depth 4 nodes ({})",
        depth5.nodes,
        depth4.nodes
    );
}

// ============================================================================
// Thread Safety and Concurrency Tests
// ============================================================================

#[test]
fn test_multiple_engines_concurrent() {
    // Test that multiple search engines can be created and used concurrently
    let mut handles = vec![];

    for _i in 0..4 {
        let handle = thread::spawn(|| -> UCIResult<()> {
            let mut board = Board::new()?;
            let mut engine = SearchEngine::new(&mut board)?;
            let _result = engine.search(SearchLimits::depth(3))?;
            Ok(())
        });
        handles.push(handle);
    }

    for handle in handles {
        handle.join().expect("Thread panicked").expect("Search failed");
    }
}

#[test]
fn test_sequential_searches() {
    let mut board = Board::new().expect("Failed to create board");
    let mut engine = SearchEngine::new(&mut board).expect("Failed to create engine");

    // Perform multiple sequential searches
    for depth in 3..=5 {
        let limits = SearchLimits::depth(depth);
        let result = engine.search(limits).expect("Search failed");
        assert!(result.is_valid(), "Search at depth {} failed", depth);

        // Verify last result is cached
        assert!(engine.get_last_result().is_some(), "Last result should be cached");
    }
}

// ============================================================================
// Edge Cases and Error Handling
// ============================================================================

#[test]
fn test_search_in_checkmate_position() {
    let mut board = Board::new().expect("Failed to create board");

    // Fool's mate position (Black checkmate)
    board
        .set_from_fen("rnb1kbnr/pppp1ppp/8/4p3/6Pq/5P2/PPPPP2P/RNBQKBNR w KQkq - 0 1")
        .expect("Failed to set FEN");

    let mut engine = SearchEngine::new(&mut board).expect("Failed to create engine");

    // Should still be able to search (even though position is mate)
    let result = engine.search(SearchLimits::depth(3));
    // Result behavior may vary - either finds mate or returns any legal move
    assert!(result.is_ok(), "Should handle checkmate position gracefully");
}

#[test]
fn test_search_after_board_moves() {
    let mut board = Board::new().expect("Failed to create board");

    // Make some moves
    board.make_move("e2e4").expect("Move failed");
    board.make_move("e7e5").expect("Move failed");
    board.make_move("g1f3").expect("Move failed");

    // Create engine after moves
    let mut engine = SearchEngine::new(&mut board).expect("Failed to create engine");
    let result = engine.search(SearchLimits::depth(4)).expect("Search failed");

    assert!(result.is_valid(), "Should search correctly after board moves");
}

#[test]
fn test_search_engine_lifecycle() {
    // Test creating multiple engines in sequence
    for _i in 0..10 {
        let mut board = Board::new().expect("Failed to create board");
        let mut engine = SearchEngine::new(&mut board).expect("Failed to create engine");
        let _result = engine.search(SearchLimits::depth(3)).expect("Search failed");
        // Engine and board should be properly cleaned up here
    }
}

// ============================================================================
// Async Search Tests (if tokio is available)
// ============================================================================

#[cfg(feature = "async")]
#[tokio::test]
async fn test_search_async_basic() {
    let mut board = Board::new().expect("Failed to create board");
    let mut engine = SearchEngine::new(&mut board).expect("Failed to create engine");

    let result = engine.search_async(SearchLimits::depth(5)).await;
    assert!(result.is_ok(), "Async search failed");

    let result = result.unwrap();
    assert!(result.is_valid(), "Invalid async search result");
}

#[cfg(feature = "async")]
#[tokio::test]
async fn test_search_async_concurrent() {
    // Create multiple async searches
    let mut handles = vec![];

    for _i in 0..4 {
        let handle = tokio::spawn(async move {
            let mut board = Board::new().expect("Failed to create board");
            let mut engine = SearchEngine::new(&mut board).expect("Failed to create engine");
            engine.search_async(SearchLimits::depth(4)).await
        });
        handles.push(handle);
    }

    for handle in handles {
        let result = handle.await.expect("Task panicked");
        assert!(result.is_ok(), "Async search failed");
    }
}

// ============================================================================
// Memory and Resource Tests
// ============================================================================

#[test]
fn test_search_engine_memory_cleanup() {
    // Create many search engines and let them go out of scope
    for _i in 0..100 {
        let mut board = Board::new().expect("Failed to create board");
        let mut engine = SearchEngine::new(&mut board).expect("Failed to create engine");
        let _result = engine.search(SearchLimits::depth(2)).expect("Search failed");
        // Should be properly cleaned up
    }
}

#[test]
fn test_search_engine_debug_display() {
    let mut board = Board::new().expect("Failed to create board");
    let engine = SearchEngine::new(&mut board).expect("Failed to create engine");

    let debug_str = format!("{:?}", engine);
    assert!(debug_str.contains("SearchEngine"), "Debug output should contain 'SearchEngine'");
}
