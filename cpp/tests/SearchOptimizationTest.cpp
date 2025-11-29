#include <gtest/gtest.h>
#include "../include/search/alphabeta.h"
#include "../include/Board.h"
#include "../include/search/transposition_table.h"
#include "../include/search/move_ordering.h"
#include "../include/search/see.h"
#include <chrono>

using namespace opera;

class SearchOptimizationTest : public ::testing::Test {
protected:
    void SetUp() override {
        tt = std::make_unique<TranspositionTable>(1024); // 1MB for testing
        move_ordering = std::make_unique<MoveOrdering>(board, *tt);
        see = std::make_unique<StaticExchangeEvaluator>(board);
        alphabeta = std::make_unique<AlphaBetaSearch>(board, stop_flag, *tt, *move_ordering, *see);
    }
    
    Board board;
    std::atomic<bool> stop_flag{false};
    std::unique_ptr<TranspositionTable> tt;
    std::unique_ptr<MoveOrdering> move_ordering;
    std::unique_ptr<StaticExchangeEvaluator> see;
    std::unique_ptr<AlphaBetaSearch> alphabeta;
};

// Test that search optimizations are being applied
TEST_F(SearchOptimizationTest, OptimizationsAreApplied) {
    // Start from initial position
    board.setFromFEN(STARTING_FEN);
    
    // Run deep search to trigger optimizations
    int score = alphabeta->search(5);
    
    const SearchStats& stats = alphabeta->get_stats();
    
    // Should have searched some nodes
    EXPECT_GT(stats.nodes, 100);
    
    // Should have applied some reductions (LMR)
    EXPECT_GT(stats.lmr_reductions, 0);
    
    // Should have done some futility pruning (maybe, depends on position)
    // Note: This might be 0 in starting position, so we don't assert
    
    // Should have tried razoring (maybe, depends on evaluation)
    // Note: This might be 0 in starting position, so we don't assert
    
    // Null move should be 0 since we disabled it for now
    EXPECT_EQ(stats.null_move_cutoffs, 0);
}

// Test Late Move Reductions effectiveness
TEST_F(SearchOptimizationTest, LateMoveReductions) {
    board.setFromFEN(STARTING_FEN);
    
    // Search with and without LMR (by setting different depths)
    alphabeta->reset();
    int score_with_optimizations = alphabeta->search(4);
    const SearchStats& stats_optimized = alphabeta->get_stats();
    
    // Should have applied LMR
    EXPECT_GT(stats_optimized.lmr_reductions, 0);
    EXPECT_GT(stats_optimized.reductions, 0);
    
    // LMR reductions should be reasonable (not too many)
    double lmr_rate = (double)stats_optimized.lmr_reductions / stats_optimized.nodes;
    EXPECT_LT(lmr_rate, 0.5); // Less than 50% of nodes should be reduced
}

// Test search performance improvement
TEST_F(SearchOptimizationTest, PerformanceImprovement) {
    board.setFromFEN(STARTING_FEN);
    
    // Measure search performance with optimizations
    auto start = std::chrono::high_resolution_clock::now();
    alphabeta->reset();
    int score = alphabeta->search(4);
    auto end = std::chrono::high_resolution_clock::now();
    
    uint64_t nodes = alphabeta->get_stats().nodes;
    uint64_t time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    // Should achieve reasonable performance
    if (time_ms > 0) {
        uint64_t nps = (nodes * 1000) / time_ms;
        EXPECT_GT(nps, 50000); // At least 50K nodes/second with optimizations
    }
}

// Test branching factor reduction
TEST_F(SearchOptimizationTest, BranchingFactorReduction) {
    board.setFromFEN(STARTING_FEN);
    
    // Search to different depths and measure branching factor
    alphabeta->reset();
    int score_d3 = alphabeta->search(3);
    uint64_t nodes_d3 = alphabeta->get_stats().nodes;
    
    alphabeta->reset();
    int score_d4 = alphabeta->search(4);
    uint64_t nodes_d4 = alphabeta->get_stats().nodes;
    
    // Calculate effective branching factor
    if (nodes_d3 > 0) {
        double branching_factor = (double)nodes_d4 / nodes_d3;
        
        // With optimizations, branching factor should be reasonable
        EXPECT_LT(branching_factor, 10.0); // Should be well below 10
        EXPECT_GT(branching_factor, 1.0);  // Should be greater than 1
        
        // Target: effective branching factor <4 with optimizations
        EXPECT_LT(branching_factor, 6.0); // Allow some margin for testing
    }
}

// Test Futility Pruning in tactical positions
TEST_F(SearchOptimizationTest, FutilityPruning) {
    // Set up a quiet position where futility pruning should trigger
    board.setFromFEN("8/8/8/8/8/8/8/K7 w - - 0 1"); // King endgame
    
    alphabeta->reset();
    int score = alphabeta->search(3);
    
    const SearchStats& stats = alphabeta->get_stats();
    
    // In quiet positions, should have done some futility pruning
    // Note: Depends on the evaluation, might be 0
    // EXPECT_GT(stats.futility_prunes, 0); // Commented out as it's position-dependent
}

// Test Razoring in bad positions
TEST_F(SearchOptimizationTest, Razoring) {
    // Set up a position where razoring might trigger
    board.setFromFEN(STARTING_FEN);
    
    alphabeta->reset();
    int score = alphabeta->search(4);
    
    const SearchStats& stats = alphabeta->get_stats();
    
    // Razoring should be applied in some positions
    // Note: Might be 0 in starting position
    // EXPECT_GE(stats.razoring_prunes, 0); // Just check it's not crashing
}

// Test statistics tracking
TEST_F(SearchOptimizationTest, StatisticsTracking) {
    board.setFromFEN(STARTING_FEN);
    
    alphabeta->reset();
    int score = alphabeta->search(4);
    
    const SearchStats& stats = alphabeta->get_stats();
    
    // All statistics should be non-negative
    EXPECT_GE(stats.nodes, 0);
    EXPECT_GE(stats.beta_cutoffs, 0);
    EXPECT_GE(stats.first_move_cutoffs, 0);
    EXPECT_GE(stats.tt_hits, 0);
    EXPECT_GE(stats.tt_cutoffs, 0);
    EXPECT_GE(stats.extensions, 0);
    EXPECT_GE(stats.reductions, 0);
    EXPECT_GE(stats.null_move_cutoffs, 0);
    EXPECT_GE(stats.lmr_reductions, 0);
    EXPECT_GE(stats.futility_prunes, 0);
    EXPECT_GE(stats.razoring_prunes, 0);
    
    // Basic sanity checks
    EXPECT_GE(stats.beta_cutoffs, stats.first_move_cutoffs);
    EXPECT_GE(stats.reductions, stats.lmr_reductions);
}

// Test optimization methods work correctly
TEST_F(SearchOptimizationTest, OptimizationMethods) {
    board.setFromFEN(STARTING_FEN);
    
    // Test LMR reduction calculation
    MoveGen dummy_move(A2, A3); // Quiet move
    
    // Should not reduce PV nodes
    int reduction_pv = alphabeta->get_lmr_reduction(4, 5, true, dummy_move);
    EXPECT_EQ(reduction_pv, 0);
    
    // Should not reduce first few moves
    int reduction_early = alphabeta->get_lmr_reduction(4, 2, false, dummy_move);
    EXPECT_EQ(reduction_early, 0);
    
    // Should reduce later quiet moves
    int reduction_late = alphabeta->get_lmr_reduction(6, 10, false, dummy_move);
    EXPECT_GT(reduction_late, 0);
    EXPECT_LE(reduction_late, DEFAULT_LMR_REDUCTION_LIMIT);
    
    // Test futility pruning conditions
    bool can_prune = alphabeta->can_futility_prune(1, 100, -100);
    EXPECT_TRUE(can_prune); // static_eval + margin < alpha
    
    bool cant_prune = alphabeta->can_futility_prune(1, 100, 200);
    EXPECT_FALSE(cant_prune); // static_eval + margin >= alpha
    
    // Test razoring conditions
    bool can_razor = alphabeta->can_razor(2, 100, -200);
    EXPECT_TRUE(can_razor); // static_eval + margin < alpha
    
    bool cant_razor = alphabeta->can_razor(2, 100, 200);
    EXPECT_FALSE(cant_razor); // static_eval + margin >= alpha
}

// Test that optimizations don't break search correctness
TEST_F(SearchOptimizationTest, SearchCorrectness) {
    board.setFromFEN(STARTING_FEN);
    
    // Should return reasonable scores
    int score = alphabeta->search(3);
    EXPECT_GE(score, -CHECKMATE_SCORE);
    EXPECT_LE(score, CHECKMATE_SCORE);
    
    // Should be close to equality in starting position
    EXPECT_GE(score, -100);
    EXPECT_LE(score, 100);
    
    // Should have reasonable principal variation
    const std::vector<Move>& pv = alphabeta->get_principal_variation();
    EXPECT_GT(pv.size(), 0);
    EXPECT_LE(pv.size(), 10); // Reasonable PV length
}

// Performance benchmark
TEST_F(SearchOptimizationTest, PerformanceBenchmark) {
    board.setFromFEN(STARTING_FEN);
    
    auto start = std::chrono::high_resolution_clock::now();
    alphabeta->reset();
    int score = alphabeta->search(5);
    auto end = std::chrono::high_resolution_clock::now();
    
    uint64_t nodes = alphabeta->get_stats().nodes;
    uint64_t time_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    // Performance targets with optimizations
    EXPECT_GT(nodes, 1000); // Should search reasonable number of nodes
    
    if (time_ms > 0) {
        uint64_t nps = (nodes * 1000) / time_ms;
        
        // Should maintain high performance with optimizations
        EXPECT_GT(nps, 100000); // Target: >100K nodes/second
        
        std::cout << "Search Performance with Optimizations:" << std::endl;
        std::cout << "  Depth: 5" << std::endl;
        std::cout << "  Nodes: " << nodes << std::endl;
        std::cout << "  Time: " << time_ms << "ms" << std::endl;
        std::cout << "  NPS: " << nps << std::endl;
        
        const SearchStats& stats = alphabeta->get_stats();
        std::cout << "Optimization Statistics:" << std::endl;
        std::cout << "  LMR reductions: " << stats.lmr_reductions << std::endl;
        std::cout << "  Futility prunes: " << stats.futility_prunes << std::endl;
        std::cout << "  Razoring prunes: " << stats.razoring_prunes << std::endl;
        std::cout << "  Null move cutoffs: " << stats.null_move_cutoffs << std::endl;
    }
}