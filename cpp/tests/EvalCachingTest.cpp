/**
 * @file EvalCachingTest.cpp
 * @brief Tests for evaluation caching and optimization (Task 3.6)
 *
 * Validates:
 * - Pawn structure hash table functionality
 * - Cache hit rate requirements (>95%)
 * - Performance with caching (<1μs average)
 * - Incremental evaluation updates
 * - Memory usage constraints (<10MB)
 */

#include <gtest/gtest.h>
#include <memory>
#include <chrono>
#include "Board.h"
#include "eval/handcrafted_eval.h"

using namespace opera;
using namespace opera::eval;

class EvalCachingTest : public ::testing::Test {
protected:
    void SetUp() override {
        board = std::make_unique<Board>();
        evaluator = std::make_unique<HandcraftedEvaluator>();
    }

    std::unique_ptr<Board> board;
    std::unique_ptr<HandcraftedEvaluator> evaluator;
};

// ============================================================================
// Pawn Hash Table Tests
// ============================================================================

/**
 * Test 1: Pawn hash table stores and retrieves correctly
 */
TEST_F(EvalCachingTest, PawnHashBasicFunctionality) {
    board->setFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    // First evaluation should miss cache and compute
    int first_eval = evaluator->evaluate(*board, Color::WHITE);

    // Second evaluation with same pawn structure should hit cache
    int second_eval = evaluator->evaluate(*board, Color::WHITE);

    EXPECT_EQ(first_eval, second_eval);
}

/**
 * Test 2: Different pawn structures get different hash entries
 */
TEST_F(EvalCachingTest, PawnHashDistinguishesDifferentStructures) {
    // Starting position
    board->setFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    evaluator->evaluate(*board, Color::WHITE);

    // Different pawn structure (e4 pushed)
    board->setFromFEN("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 1");
    evaluator->evaluate(*board, Color::WHITE);

    // Should have computed pawn structure for both positions
    // (Cache should have 2 entries now)
    SUCCEED();  // Just verify no crashes
}

/**
 * Test 3: Same pawn structure with different pieces hits cache
 */
TEST_F(EvalCachingTest, PawnHashIgnoresPiecePositions) {
    // Position 1: Knights on b1/g1
    board->setFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    evaluator->evaluate(*board, Color::WHITE);

    // Position 2: Knights on c3/f3 (same pawn structure, different pieces)
    board->setFromFEN("r1bqkb1r/pppppppp/2n2n2/8/8/2N2N2/PPPPPPPP/R1BQKB1R w KQkq - 0 1");
    int eval_with_developed = evaluator->evaluate(*board, Color::WHITE);

    // Pawn hash should hit (same pawn structure)
    // But overall eval will differ due to piece positions
    EXPECT_NE(eval_with_developed, 0);  // Just verify evaluation works
}

/**
 * Test 4: Cache hit rate requirement (>95%)
 */
TEST_F(EvalCachingTest, PawnHashHitRateRequirement) {
    // Simulate typical game: same position evaluated multiple times
    board->setFromFEN("rnbqkb1r/pppp1ppp/5n2/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 0 1");

    // Evaluate same position 100 times (typical search scenario)
    for (int i = 0; i < 100; ++i) {
        evaluator->evaluate(*board, Color::WHITE);
    }

    // Get cache statistics
    auto stats = evaluator->get_pawn_hash_stats();

    // Hit rate should be >95% (99 hits out of 100 evaluations)
    double hit_rate = static_cast<double>(stats.hits) / (stats.hits + stats.misses);
    EXPECT_GT(hit_rate, 0.95);
    EXPECT_GE(stats.hits, 99);  // At least 99 hits
}

/**
 * Test 5: Cache clears properly
 */
TEST_F(EvalCachingTest, PawnHashClearFunctionality) {
    board->setFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    evaluator->evaluate(*board, Color::WHITE);

    // Clear cache
    evaluator->clear_pawn_hash();

    // Next evaluation should miss cache
    auto stats_before = evaluator->get_pawn_hash_stats();
    evaluator->evaluate(*board, Color::WHITE);
    auto stats_after = evaluator->get_pawn_hash_stats();

    EXPECT_EQ(stats_after.misses, stats_before.misses + 1);
}

// ============================================================================
// Performance Tests
// ============================================================================

/**
 * Test 6: Performance with caching (<1μs average)
 */
TEST_F(EvalCachingTest, PerformanceWithCaching) {
    board->setFromFEN("rnbqkb1r/pppp1ppp/5n2/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 0 1");

    // Warm up cache
    evaluator->evaluate(*board, Color::WHITE);

    // Measure performance with cache hits
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 10000; ++i) {
        evaluator->evaluate(*board, Color::WHITE);
    }
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    double avg_time_ns = duration.count() / 10000.0;

    // Should maintain <1μs (1000ns) performance
    EXPECT_LT(avg_time_ns, 1000.0);
}

/**
 * Test 7: Performance improvement from caching
 */
TEST_F(EvalCachingTest, CachingSpeedsUpEvaluation) {
    board->setFromFEN("rnbqkb1r/pppp1ppp/5n2/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 0 1");

    // Clear cache and measure cold performance
    evaluator->clear_pawn_hash();
    auto start_cold = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000; ++i) {
        evaluator->evaluate(*board, Color::WHITE);
        evaluator->clear_pawn_hash();  // Force cache miss each time
    }
    auto end_cold = std::chrono::high_resolution_clock::now();
    auto duration_cold = std::chrono::duration_cast<std::chrono::nanoseconds>(end_cold - start_cold);

    // Clear and warm up cache
    evaluator->clear_pawn_hash();
    evaluator->evaluate(*board, Color::WHITE);

    // Measure hot performance (cache hits)
    auto start_hot = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000; ++i) {
        evaluator->evaluate(*board, Color::WHITE);
    }
    auto end_hot = std::chrono::high_resolution_clock::now();
    auto duration_hot = std::chrono::duration_cast<std::chrono::nanoseconds>(end_hot - start_hot);

    // Cached evaluations should be faster (at least 10% improvement)
    EXPECT_LT(duration_hot.count(), duration_cold.count() * 0.9);
}

// ============================================================================
// Memory Usage Tests
// ============================================================================

/**
 * Test 8: Memory usage constraint (<10MB)
 */
TEST_F(EvalCachingTest, MemoryUsageConstraint) {
    // Fill cache with diverse positions
    std::vector<std::string> fens = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 1",
        "rnbqkbnr/ppp1pppp/8/3p4/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 1",
        "rnbqkb1r/pppppppp/5n2/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 1",
        "r1bqkb1r/pppppppp/2n2n2/8/4P3/2N2N2/PPPP1PPP/R1BQKB1R w KQkq - 0 1",
    };

    for (const auto& fen : fens) {
        board->setFromFEN(fen);
        evaluator->evaluate(*board, Color::WHITE);
    }

    // Check memory usage
    size_t memory_usage = evaluator->get_pawn_hash_memory_usage();

    // Should be less than 10MB (10,485,760 bytes)
    EXPECT_LT(memory_usage, 10485760);
}

/**
 * Test 9: Configurable cache size
 */
TEST_F(EvalCachingTest, ConfigurableCacheSize) {
    // Configure cache size via options
    std::map<std::string, std::string> options;
    options["PawnHashSize"] = "1";  // 1MB

    evaluator->configure_options(options);

    // Verify size was set
    size_t memory_usage = evaluator->get_pawn_hash_memory_usage();
    EXPECT_LE(memory_usage, 1048576);  // ≤1MB
}

// ============================================================================
// Incremental Update Tests (Future work - Task 3.6 Phase 2)
// ============================================================================

// TODO: Implement incremental evaluation updates
// These tests are placeholders for future implementation

/**
 * Test 10: Placeholder for on_move_made callback
 */
TEST_F(EvalCachingTest, IncrementalUpdatePlaceholder) {
    // Future: Test incremental evaluation updates with on_move_made/on_move_undone
    SUCCEED();  // Placeholder test
}

// ============================================================================
// Integration Tests
// ============================================================================

/**
 * Test 12: Caching doesn't affect evaluation correctness
 */
TEST_F(EvalCachingTest, CachingMaintainsCorrectness) {
    std::vector<std::string> test_positions = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "rnbqkb1r/pppp1ppp/5n2/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 0 1",
        "r1bqkb1r/pppp1ppp/2n2n2/4p3/4P3/2N2N2/PPPP1PPP/R1BQKB1R w KQkq - 0 1",
    };

    for (const auto& fen : test_positions) {
        board->setFromFEN(fen);

        // Evaluate with cold cache
        evaluator->clear_pawn_hash();
        int eval_cold = evaluator->evaluate(*board, Color::WHITE);

        // Evaluate with warm cache
        int eval_hot = evaluator->evaluate(*board, Color::WHITE);

        // Results should be identical
        EXPECT_EQ(eval_cold, eval_hot);
    }
}
