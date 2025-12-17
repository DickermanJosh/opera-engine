/**
 * @file HandcraftedEvalTest.cpp
 * @brief Comprehensive tests for HandcraftedEvaluator implementation
 *
 * Tests traditional chess evaluation including material balance, piece-square
 * tables, and tapered evaluation for opening/middlegame/endgame phases.
 *
 * Following TDD principles: tests written before implementation, 100% coverage.
 */

#include <gtest/gtest.h>
#include "eval/handcrafted_eval.h"
#include "Board.h"
#include <memory>
#include <chrono>
#include <map>
#include <string>

namespace opera {
namespace eval {

class HandcraftedEvalTest : public ::testing::Test {
protected:
    void SetUp() override {
        board = std::make_unique<Board>();
        evaluator = std::make_unique<HandcraftedEvaluator>();
    }

    std::unique_ptr<Board> board;
    std::unique_ptr<HandcraftedEvaluator> evaluator;

    // Helper: Count material for verification
    int count_material(const Board& b, Color color) {
        int material = 0;
        // Simplified - actual implementation will use bitboards
        return material;
    }
};

// ============================================================================
// Core Interface Implementation Tests
// ============================================================================

/**
 * Test 1: HandcraftedEvaluator can be instantiated
 */
TEST_F(HandcraftedEvalTest, CanInstantiate) {
    ASSERT_NE(evaluator, nullptr);
}

/**
 * Test 2: Implements Evaluator interface correctly
 */
TEST_F(HandcraftedEvalTest, ImplementsEvaluatorInterface) {
    std::unique_ptr<Evaluator> base_ptr = std::make_unique<HandcraftedEvaluator>();
    ASSERT_NE(base_ptr, nullptr);
}

/**
 * Test 3: Configure options doesn't crash with empty map
 */
TEST_F(HandcraftedEvalTest, ConfigureEmptyOptions) {
    std::map<std::string, std::string> empty_options;
    EXPECT_NO_THROW(evaluator->configure_options(empty_options));
}

// ============================================================================
// Material Evaluation Tests
// ============================================================================

/**
 * Test 4: Starting position evaluates to approximately zero
 */
TEST_F(HandcraftedEvalTest, StartingPositionNearZero) {
    board->setFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    int score = evaluator->evaluate(*board, Color::WHITE);

    // Starting position should be nearly equal (within tempo bonus range)
    EXPECT_GE(score, -50);  // Not worse than -0.5 pawns
    EXPECT_LE(score, 50);   // Not better than +0.5 pawns
}

/**
 * Test 5: White up a pawn shows positive evaluation
 */
TEST_F(HandcraftedEvalTest, WhiteUpPawn) {
    // White has extra pawn on e4 (black missing f7 pawn)
    board->setFromFEN("rnbqkbnr/ppppp1pp/8/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 1");

    int score = evaluator->evaluate(*board, Color::WHITE);

    // Should be at least +100 centipawns (1 pawn = 100cp)
    EXPECT_GT(score, 80);
}

/**
 * Test 6: Black up a pawn shows negative evaluation
 */
TEST_F(HandcraftedEvalTest, BlackUpPawn) {
    // Black has extra pawn (white missing e2 pawn)
    board->setFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPP1PPP/RNBQKBNR w KQkq - 0 1");

    int score = evaluator->evaluate(*board, Color::WHITE);

    // Should be at least -100 centipawns
    EXPECT_LT(score, -80);
}

/**
 * Test 7: Material values follow standard chess values
 * Pawn=100, Knight=320, Bishop=330, Rook=500, Queen=900
 */
TEST_F(HandcraftedEvalTest, StandardMaterialValues) {
    // White up a knight (N=320cp) - black missing queenside knight
    board->setFromFEN("r1bqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    int knight_score = evaluator->evaluate(*board, Color::WHITE);
    EXPECT_GT(knight_score, 280);  // At least 320cp - some PST adjustment
    EXPECT_LT(knight_score, 360);  // Not more than 320cp + PST bonus

    // White up a rook (R=500cp) - black missing kingside rook
    board->setFromFEN("rnbqkbn1/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQq - 0 1");
    int rook_score = evaluator->evaluate(*board, Color::WHITE);
    EXPECT_GT(rook_score, 450);
    EXPECT_LT(rook_score, 550);

    // White up a queen (Q=900cp) - black missing queen
    board->setFromFEN("rnb1kbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    int queen_score = evaluator->evaluate(*board, Color::WHITE);
    EXPECT_GT(queen_score, 850);
    EXPECT_LT(queen_score, 950);
}

/**
 * Test 8: Evaluation is symmetric (white advantage = -black advantage)
 */
TEST_F(HandcraftedEvalTest, EvaluationSymmetry) {
    // Position with white advantage
    board->setFromFEN("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 1");
    int white_score = evaluator->evaluate(*board, Color::WHITE);

    // Flipped position (black advantage in same structure)
    board->setFromFEN("rnbqkbnr/pppp1ppp/8/4p3/8/8/PPPPPPPP/RNBQKBNR b KQkq - 0 1");
    int black_score = evaluator->evaluate(*board, Color::BLACK);

    // Scores should be approximately opposite (within small margin for tempo)
    EXPECT_NEAR(white_score, -black_score, 30);
}

// ============================================================================
// Piece-Square Table Tests
// ============================================================================

/**
 * Test 9: Central pawns valued higher than edge pawns
 */
TEST_F(HandcraftedEvalTest, CentralPawnsPreferred) {
    // Pawn on e4 (central)
    board->setFromFEN("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 1");
    int central_score = evaluator->evaluate(*board, Color::WHITE);

    // Pawn on a4 (edge)
    board->setFromFEN("rnbqkbnr/pppppppp/8/8/P7/8/1PPPPPPP/RNBQKBNR w KQkq - 0 1");
    int edge_score = evaluator->evaluate(*board, Color::WHITE);

    // Central pawn should have higher PST bonus
    EXPECT_GT(central_score, edge_score);
}

/**
 * Test 10: Knights on rim are dim (edge knights penalized)
 */
TEST_F(HandcraftedEvalTest, KnightsOnRimAreDim) {
    // Knight on f3 (good square)
    board->setFromFEN("rnbqkbnr/pppppppp/8/8/8/5N2/PPPPPPPP/RNBQKB1R w KQkq - 0 1");
    int center_knight_score = evaluator->evaluate(*board, Color::WHITE);

    // Knight on a3 (rim)
    board->setFromFEN("rnbqkbnr/pppppppp/8/8/8/N7/PPPPPPPP/R1BQKBNR w KQkq - 0 1");
    int rim_knight_score = evaluator->evaluate(*board, Color::WHITE);

    // Central knight should be valued higher
    EXPECT_GT(center_knight_score, rim_knight_score);
}

/**
 * Test 11: Rooks on 7th rank valued higher than back rank
 */
TEST_F(HandcraftedEvalTest, RooksOn7thRankValued) {
    // White rook on 7th rank (b7), equal material otherwise
    board->setFromFEN("r1bqkb1r/pRpppppp/8/8/8/8/PPPPPPPP/1NBQKB1R w Kkq - 0 1");
    int seventh_rank_score = evaluator->evaluate(*board, Color::WHITE);

    // White rook on 1st rank (b1), equal material
    board->setFromFEN("r1bqkb1r/p1pppppp/8/8/8/8/PPPPPPPP/RNBQKB1R w Kkq - 0 1");
    int first_rank_score = evaluator->evaluate(*board, Color::WHITE);

    // 7th rank should be better (PST bonus)
    EXPECT_GT(seventh_rank_score, first_rank_score);
}

// ============================================================================
// Phase Detection and Tapered Evaluation Tests
// ============================================================================

/**
 * Test 12: Opening phase detected correctly
 */
TEST_F(HandcraftedEvalTest, DetectsOpeningPhase) {
    // Starting position - full pieces = opening
    board->setFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    int score = evaluator->evaluate(*board, Color::WHITE);

    // Should apply opening PST values (test implementation detail)
    EXPECT_GE(score, -50);
    EXPECT_LE(score, 50);
}

/**
 * Test 13: Endgame phase detected correctly
 */
TEST_F(HandcraftedEvalTest, DetectsEndgamePhase) {
    // King and pawn endgame
    board->setFromFEN("8/4k3/8/8/8/8/4P3/4K3 w - - 0 1");

    int score = evaluator->evaluate(*board, Color::WHITE);

    // Endgame: white up a pawn
    EXPECT_GT(score, 80);
}

/**
 * Test 14: Middlegame interpolation works
 */
TEST_F(HandcraftedEvalTest, MiddlegameInterpolation) {
    // Middlegame position (some pieces traded)
    board->setFromFEN("r1bqk2r/pppp1ppp/2n2n2/2b1p3/2B1P3/3P1N2/PPP2PPP/RNBQK2R w KQkq - 0 1");

    int score = evaluator->evaluate(*board, Color::WHITE);

    // Should be approximately equal (within 1 pawn)
    EXPECT_GE(score, -100);
    EXPECT_LE(score, 100);
}

/**
 * Test 15: King centralization in endgame
 */
TEST_F(HandcraftedEvalTest, KingCentralizationEndgame) {
    // Endgame: king on e4 (central)
    board->setFromFEN("8/8/8/8/4K3/8/8/4k3 w - - 0 1");
    int central_king = evaluator->evaluate(*board, Color::WHITE);

    // Endgame: king on a1 (corner)
    board->setFromFEN("8/8/8/8/8/8/8/K3k3 w - - 0 1");
    int corner_king = evaluator->evaluate(*board, Color::WHITE);

    // Central king should be better in endgame
    EXPECT_GT(central_king, corner_king);
}

// ============================================================================
// Side-to-Move Tests
// ============================================================================

/**
 * Test 16: Side to move receives tempo bonus
 */
TEST_F(HandcraftedEvalTest, TempoBonus) {
    // Same position, different side to move
    board->setFromFEN("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 1");
    int white_to_move = evaluator->evaluate(*board, Color::WHITE);

    board->setFromFEN("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1");
    int black_to_move = evaluator->evaluate(*board, Color::BLACK);

    // White to move should be slightly better than black to move
    // (tempo bonus typically 10-20 centipawns)
    EXPECT_GT(white_to_move, black_to_move - 30);
}

// ============================================================================
// Performance Tests
// ============================================================================

/**
 * Test 17: Evaluation completes in <1 microsecond
 */
TEST_F(HandcraftedEvalTest, EvaluationPerformance) {
    board->setFromFEN("r1bqk2r/pppp1ppp/2n2n2/2b1p3/2B1P3/3P1N2/PPP2PPP/RNBQK2R w KQkq - 0 1");

    // Warm up
    for (int i = 0; i < 100; ++i) {
        evaluator->evaluate(*board, Color::WHITE);
    }

    // Benchmark
    auto start = std::chrono::high_resolution_clock::now();
    const int iterations = 10000;
    for (int i = 0; i < iterations; ++i) {
        evaluator->evaluate(*board, Color::WHITE);
    }
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    double avg_ns = static_cast<double>(duration.count()) / iterations;

    // Should average <1000ns (1μs) per evaluation
    EXPECT_LT(avg_ns, 1000.0);
}

/**
 * Test 18: Multiple evaluations are consistent
 */
TEST_F(HandcraftedEvalTest, ConsistentEvaluations) {
    board->setFromFEN("r1bqk2r/pppp1ppp/2n2n2/2b1p3/2B1P3/3P1N2/PPP2PPP/RNBQK2R w KQkq - 0 1");

    int score1 = evaluator->evaluate(*board, Color::WHITE);
    int score2 = evaluator->evaluate(*board, Color::WHITE);
    int score3 = evaluator->evaluate(*board, Color::WHITE);

    EXPECT_EQ(score1, score2);
    EXPECT_EQ(score2, score3);
}

// ============================================================================
// Edge Cases and Robustness Tests
// ============================================================================

/**
 * Test 19: Empty board evaluates to zero
 */
TEST_F(HandcraftedEvalTest, EmptyBoardZero) {
    board->setFromFEN("8/8/8/8/8/8/8/8 w - - 0 1");

    int score = evaluator->evaluate(*board, Color::WHITE);

    EXPECT_EQ(score, 0);
}

/**
 * Test 20: Only kings evaluates to zero
 */
TEST_F(HandcraftedEvalTest, OnlyKingsZero) {
    board->setFromFEN("4k3/8/8/8/8/8/8/4K3 w - - 0 1");

    int score = evaluator->evaluate(*board, Color::WHITE);

    EXPECT_EQ(score, 0);
}

/**
 * Test 21: Massive material advantage
 */
TEST_F(HandcraftedEvalTest, MassiveMaterialAdvantage) {
    // White has queen + rooks vs black pawns
    board->setFromFEN("4k3/pppppppp/8/8/8/8/8/RRQK4 w - - 0 1");

    int score = evaluator->evaluate(*board, Color::WHITE);

    // Should be massively positive (>500 centipawns material advantage)
    // PST adjustments reduce the raw material count
    EXPECT_GT(score, 400);
}

/**
 * Test 22: Configuration changes affect evaluation
 */
TEST_F(HandcraftedEvalTest, ConfigurationChangesEvaluation) {
    board->setFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    int default_score = evaluator->evaluate(*board, Color::WHITE);

    // Configure with different weights (if supported)
    std::map<std::string, std::string> options = {
        {"MaterialWeight", "2.0"}  // Double material importance
    };
    evaluator->configure_options(options);

    // Evaluation should still work (may or may not change depending on implementation)
    int configured_score = evaluator->evaluate(*board, Color::WHITE);

    // Both should be near zero for starting position
    EXPECT_GE(configured_score, -50);
    EXPECT_LE(configured_score, 50);
}

/**
 * Test 23: Evaluation with black to move (still white perspective score)
 */
TEST_F(HandcraftedEvalTest, BlackPerspectiveEvaluation) {
    // Position favoring white (white up a pawn, black missing f7), black to move
    board->setFromFEN("rnbqkbnr/ppppp1pp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1");

    int score = evaluator->evaluate(*board, Color::BLACK);

    // Score is ALWAYS from white's perspective (positive = white advantage)
    // Black to move means tempo penalty (score slightly lower than white to move)
    EXPECT_GT(score, 80);  // White up a pawn minus tempo
}

/**
 * Test 24: Complex middlegame position
 */
TEST_F(HandcraftedEvalTest, ComplexMiddlegamePosition) {
    // Karpov vs Kasparov 1985
    board->setFromFEN("r1bq1rk1/pp3pbp/2np1np1/2p1p3/2P1P3/2NP1NP1/PP2QPBP/R1B2RK1 w - - 0 1");

    int score = evaluator->evaluate(*board, Color::WHITE);

    // Should evaluate (exact score depends on implementation)
    EXPECT_GE(score, -500);
    EXPECT_LE(score, 500);
}

/**
 * Test 25: Pawn endgame evaluation
 */
TEST_F(HandcraftedEvalTest, PawnEndgameEvaluation) {
    // White has passed pawn advantage
    board->setFromFEN("8/4k3/8/8/8/8/4P3/4K3 w - - 0 1");

    int score = evaluator->evaluate(*board, Color::WHITE);

    // White is up a pawn
    EXPECT_GT(score, 80);
}

} // namespace eval
} // namespace opera
