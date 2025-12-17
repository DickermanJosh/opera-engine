#include <gtest/gtest.h>
#include <memory>
#include <atomic>
#include "search/alphabeta.h"
#include "search/transposition_table.h"
#include "search/move_ordering.h"
#include "search/see.h"
#include "Board.h"

using namespace opera;

class AlphaBetaTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize board with starting position
        board = std::make_unique<Board>();
        board->setFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
        
        // Initialize atomic stop flag
        stop_flag = std::make_unique<std::atomic<bool>>(false);
        
        // Initialize required components
        tt = std::make_unique<TranspositionTable>(1); // 1MB table
        move_ordering = std::make_unique<MoveOrdering>(*board, *tt);
        see = std::make_unique<StaticExchangeEvaluator>(*board);
        
        // Create alpha-beta search instance
        search = std::make_unique<AlphaBetaSearch>(*board, *stop_flag, *tt, *move_ordering, *see);
    }

    void TearDown() override {
        search.reset();
        see.reset();
        move_ordering.reset();
        tt.reset();
        stop_flag.reset();
        board.reset();
    }

    // Helper to set board from FEN
    void setPosition(const std::string& fen) {
        board->setFromFEN(fen);
        // Update components that depend on board state
        move_ordering = std::make_unique<MoveOrdering>(*board, *tt);
        see = std::make_unique<StaticExchangeEvaluator>(*board);
        search = std::make_unique<AlphaBetaSearch>(*board, *stop_flag, *tt, *move_ordering, *see);
    }

    std::unique_ptr<Board> board;
    std::unique_ptr<std::atomic<bool>> stop_flag;
    std::unique_ptr<TranspositionTable> tt;
    std::unique_ptr<MoveOrdering> move_ordering;
    std::unique_ptr<StaticExchangeEvaluator> see;
    std::unique_ptr<AlphaBetaSearch> search;
};

// Basic Construction and Interface Tests

TEST_F(AlphaBetaTest, Construction) {
    // Test that AlphaBetaSearch can be constructed successfully
    EXPECT_NE(search, nullptr);
    EXPECT_EQ(search->get_stats().nodes, 0);
    EXPECT_TRUE(search->get_principal_variation().empty());
}

TEST_F(AlphaBetaTest, ResetFunctionality) {
    // Perform a small search to generate some stats
    int score = search->search(2);
    EXPECT_GT(search->get_stats().nodes, 0);
    
    // Reset and verify state is cleared
    search->reset();
    EXPECT_EQ(search->get_stats().nodes, 0);
    EXPECT_TRUE(search->get_principal_variation().empty());
}

TEST_F(AlphaBetaTest, ClearHistory) {
    // Test that clear_history doesn't crash
    search->clear_history();
    
    // Should still be able to search after clearing history
    int score = search->search(1);
    EXPECT_GT(search->get_stats().nodes, 0);
}

// Basic Search Functionality Tests

TEST_F(AlphaBetaTest, DepthZeroSearch) {
    // Depth 0 should return evaluation of current position
    int score = search->search(0);
    
    // Starting position should be roughly equal (within 50cp)
    EXPECT_NEAR(score, 0, 50);
    EXPECT_EQ(search->get_stats().nodes, 1); // Only root node evaluated
}

TEST_F(AlphaBetaTest, DepthOneSearch) {
    // Depth 1 should examine all legal moves from root
    int score = search->search(1);
    
    // Should have searched root + all legal moves
    EXPECT_GT(search->get_stats().nodes, 20); // At least 20 moves from start
    EXPECT_LT(search->get_stats().nodes, 200); // But not too many
    
    // Starting position evaluation should be reasonable
    EXPECT_GT(score, -100);
    EXPECT_LT(score, 100);
}

TEST_F(AlphaBetaTest, DepthTwoSearch) {
    // Depth 2 should examine moves and responses
    int score = search->search(2);
    
    // Should have searched significantly more nodes
    EXPECT_GT(search->get_stats().nodes, 100);
    EXPECT_LT(search->get_stats().nodes, 5000); // Reasonable upper bound
}

TEST_F(AlphaBetaTest, PrincipalVariation) {
    // Search should produce a principal variation
    search->search(3);
    const auto& pv = search->get_principal_variation();
    
    // PV should not be empty and should be reasonable length
    EXPECT_FALSE(pv.empty());
    EXPECT_LE(pv.size(), 3); // At most depth moves
}

// Alpha-Beta Bounds Testing

TEST_F(AlphaBetaTest, AlphaBetaBounds) {
    // Test that search respects alpha-beta bounds
    int full_search = search->search(2, -INFINITY_SCORE, INFINITY_SCORE);
    
    search->reset();
    
    // Narrow window search
    int narrow_search = search->search(2, full_search - 10, full_search + 10);
    
    // Should be within bounds (if not fail-high/low)
    if (narrow_search > full_search - 10 && narrow_search < full_search + 10) {
        EXPECT_EQ(narrow_search, full_search);
    }
}

TEST_F(AlphaBetaTest, FailHigh) {
    // Set up position where search should fail high
    search->reset();
    
    int score = search->search(2, -50, -40); // Narrow window expecting fail-high
    
    // If position is better than -40, should fail high (return >= -40)
    // Exact behavior depends on position, just test it doesn't crash
    EXPECT_GT(search->get_stats().nodes, 0);
}

TEST_F(AlphaBetaTest, FailLow) {
    // Set up position where search should fail low
    search->reset();
    
    int score = search->search(2, 40, 50); // Narrow window expecting fail-low
    
    // If position is worse than 40, should fail low (return <= 40)
    // Exact behavior depends on position, just test it doesn't crash
    EXPECT_GT(search->get_stats().nodes, 0);
}

// Quiescence Search Tests

TEST_F(AlphaBetaTest, QuiescenceSearch) {
    // Test quiescence search directly
    int q_score = search->quiescence(0, -INFINITY_SCORE, INFINITY_SCORE);
    
    // Should have evaluated at least the current position
    EXPECT_GT(search->get_stats().nodes, 0);
    
    // Score should be reasonable for starting position
    EXPECT_GT(q_score, -500);
    EXPECT_LT(q_score, 500);
}

TEST_F(AlphaBetaTest, QuiescenceVsTacticalPosition) {
    // Set up a tactical position with captures available
    setPosition("r1bqk2r/pp2nppp/2n1p3/2ppP3/3P4/2N2N2/PPP2PPP/R1BQKB1R w KQkq - 0 1");
    
    // Quiescence should resolve tactics
    int q_score = search->quiescence(0, -INFINITY_SCORE, INFINITY_SCORE);
    
    // Should search some captures
    EXPECT_GT(search->get_stats().nodes, 1);
}

// PVS (Principal Variation Search) Tests

TEST_F(AlphaBetaTest, PVSvsFullWindow) {
    // Compare PVS search with full alpha-beta
    int pvs_score = search->search(3);
    uint64_t pvs_nodes = search->get_stats().nodes;
    
    // PVS should be efficient (fewer nodes than naive alpha-beta)
    // This is hard to test directly without a naive implementation
    // Just verify it produces reasonable results
    EXPECT_GT(pvs_nodes, 0);
    EXPECT_GT(pvs_score, -1000);
    EXPECT_LT(pvs_score, 1000);
}

TEST_F(AlphaBetaTest, PVConsistency) {
    // PV should be consistent between searches of same position
    int score1 = search->search(3);
    auto pv1 = search->get_principal_variation();
    
    search->reset();
    
    int score2 = search->search(3);
    auto pv2 = search->get_principal_variation();
    
    // Scores should be identical
    EXPECT_EQ(score1, score2);
    
    // PVs should have same first move (deterministic)
    if (!pv1.empty() && !pv2.empty()) {
        EXPECT_EQ(pv1[0].data, pv2[0].data);
    }
}

// Search Extensions Tests

TEST_F(AlphaBetaTest, CheckExtension) {
    // Set up position with check opportunity
    setPosition("r1bqkb1r/pppp1ppp/2n2n2/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4");
    
    // Search with extensions
    search->search(4);
    
    // Should have applied some extensions
    EXPECT_GT(search->get_stats().extensions, 0);
}

TEST_F(AlphaBetaTest, ExtensionImpactsNodes) {
    // Extensions should generally increase node count
    setPosition("r1bqkb1r/pppp1ppp/2n2n2/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4");
    
    search->search(3);
    uint64_t nodes_with_extensions = search->get_stats().nodes;
    uint64_t extensions = search->get_stats().extensions;
    
    // If extensions were applied, should have searched more nodes
    if (extensions > 0) {
        EXPECT_GT(nodes_with_extensions, 100);
    }
}

// Performance and Statistics Tests

TEST_F(AlphaBetaTest, NodeCountIncrease) {
    // Node count should increase exponentially with depth
    uint64_t nodes_d1, nodes_d2, nodes_d3;
    
    search->reset();
    search->search(1);
    nodes_d1 = search->get_stats().nodes;
    
    search->reset();
    search->search(2);
    nodes_d2 = search->get_stats().nodes;
    
    search->reset();
    search->search(3);
    nodes_d3 = search->get_stats().nodes;
    
    // Each depth should search more nodes
    EXPECT_GT(nodes_d2, nodes_d1);
    EXPECT_GT(nodes_d3, nodes_d2);
    
    // Exponential growth (rough check)
    EXPECT_GT(nodes_d3, nodes_d2 * 2);
}

TEST_F(AlphaBetaTest, BetaCutoffs) {
    // Search should produce beta cutoffs (pruning)
    search->search(4);
    
    EXPECT_GT(search->get_stats().beta_cutoffs, 0);
    
    // Move ordering effectiveness should be reasonable
    double effectiveness = search->get_stats().get_move_ordering_effectiveness();
    EXPECT_GE(effectiveness, 0.0);
    EXPECT_LE(effectiveness, 1.0);
}

TEST_F(AlphaBetaTest, TranspositionTableIntegration) {
    // Search should use transposition table
    search->search(4);
    
    // Should have some TT interactions
    EXPECT_GT(search->get_stats().tt_hits + search->get_stats().tt_cutoffs, 0);
}

// Stop Flag and Search Control Tests

TEST_F(AlphaBetaTest, StopFlagRespect) {
    // Set stop flag during search
    stop_flag->store(true);
    
    int score = search->search(5); // Would take a while without stop
    
    // Search should terminate quickly
    EXPECT_LT(search->get_stats().nodes, 10000);
}

TEST_F(AlphaBetaTest, LargeDepthWithStop) {
    // Large depth search should respect stop flag
    std::atomic<bool> delayed_stop(false);
    
    // Start search in background (simulated)
    delayed_stop.store(true); // Stop immediately
    
    auto start_time = std::chrono::high_resolution_clock::now();
    stop_flag->store(true);
    int score = search->search(10);
    auto end_time = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    // Should stop quickly (within 100ms)
    EXPECT_LT(duration.count(), 100);
}

// Specific Chess Positions Tests

TEST_F(AlphaBetaTest, CheckmateInOne) {
    // Set up mate in 1 position
    setPosition("k7/8/1K6/8/8/8/8/7R w - - 0 1"); // Rh8#
    
    int score = search->search(3);
    
    // Should find mate (score > 29000)
    EXPECT_GT(score, CHECKMATE_SCORE - 10);
}

TEST_F(AlphaBetaTest, AvoidMateInOne) {
    // Position where opponent has mate in 1
    setPosition("7r/8/8/8/8/8/1k6/K7 b - - 0 1"); // Black to move, Ra8#
    
    int score = search->search(3);
    
    // Should see the mate threat (score < -29000)
    EXPECT_LT(score, -CHECKMATE_SCORE + 10);
}

TEST_F(AlphaBetaTest, TacticalPosition) {
    // Position with clear tactical win
    setPosition("r1bqkbnr/pppp1ppp/2n5/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4");
    
    search->search(5);
    
    // Should search reasonable number of nodes
    EXPECT_GT(search->get_stats().nodes, 1000);
    EXPECT_LT(search->get_stats().nodes, 100000);
    
    // Should have found some tactics (extensions, cutoffs)
    EXPECT_GT(search->get_stats().beta_cutoffs, 10);
}

TEST_F(AlphaBetaTest, EndgamePosition) {
    // Simple endgame position
    setPosition("8/8/8/8/8/3k4/3P4/3K4 w - - 0 1"); // King and pawn vs King
    
    int score = search->search(6);
    
    // Should evaluate as winning for white
    EXPECT_GT(score, 100);
    
    // Endgame should be relatively quick to search
    EXPECT_LT(search->get_stats().nodes, 50000);
}

// Integration with Other Components Tests

TEST_F(AlphaBetaTest, MoveOrderingIntegration) {
    // Test that move ordering is being used
    search->search(4);
    
    // Should have reasonable move ordering effectiveness
    auto stats = search->get_stats();
    if (stats.beta_cutoffs > 0) {
        double effectiveness = stats.get_move_ordering_effectiveness();
        EXPECT_GT(effectiveness, 0.2); // At least 20% first move cutoffs
    }
}

TEST_F(AlphaBetaTest, SEEIntegration) {
    // Position with good and bad captures
    setPosition("r1bqkb1r/pppp1ppp/2n2n2/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 4");
    
    search->search(4);
    
    // Search should complete successfully with SEE integration
    EXPECT_GT(search->get_stats().nodes, 100);
}

// Error Conditions and Edge Cases

TEST_F(AlphaBetaTest, EmptyPosition) {
    // Test with unusual position (shouldn't crash)
    try {
        setPosition("8/8/8/8/8/8/8/k6K w - - 0 1"); // Minimal legal position
        
        int score = search->search(2);
        
        // Should complete without crashing
        EXPECT_GT(search->get_stats().nodes, 0);
        
    } catch (...) {
        FAIL() << "Search should handle unusual positions gracefully";
    }
}

TEST_F(AlphaBetaTest, MaxDepthHandling) {
    // Test search at maximum depth
    int score = search->search(MAX_PLY - 1);
    
    // Should complete without stack overflow
    EXPECT_GT(search->get_stats().nodes, 0);
}

// Performance Requirements Tests

TEST_F(AlphaBetaTest, NodesPerSecondMinimum) {
    // Search should achieve >100K nodes/second minimum
    auto start_time = std::chrono::high_resolution_clock::now();
    
    search->search(5);
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    uint64_t nodes = search->get_stats().nodes;
    double nps = (double)nodes / (duration.count() / 1000000.0);
    
    // Minimum 100K NPS requirement
    if (nodes > 10000) { // Only test if significant search was done
        EXPECT_GT(nps, 100000) << "Achieved " << nps << " NPS with " << nodes << " nodes";
    }
}

TEST_F(AlphaBetaTest, SearchEfficiency) {
    // Alpha-beta should be significantly more efficient than minimax
    // Test by comparing node counts at different depths
    
    uint64_t d3_nodes, d4_nodes;
    
    search->search(3);
    d3_nodes = search->get_stats().nodes;
    
    search->reset();
    search->search(4);
    d4_nodes = search->get_stats().nodes;
    
    // Branching factor should be reasonable (<10 effective)
    double branching_factor = (double)d4_nodes / d3_nodes;
    EXPECT_LT(branching_factor, 10.0) << "Branching factor too high: " << branching_factor;
    EXPECT_GT(branching_factor, 2.0) << "Branching factor too low: " << branching_factor;
}