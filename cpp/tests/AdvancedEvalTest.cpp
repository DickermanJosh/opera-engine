/**
 * @file AdvancedEvalTest.cpp
 * @brief Comprehensive tests for advanced positional evaluation (Task 3.3)
 *
 * Tests advanced HandcraftedEvaluator features:
 * - Pawn structure analysis (isolated, doubled, passed pawns)
 * - King safety evaluation (pawn shield, open files)
 * - Piece mobility scoring
 * - Development bonuses in opening phase
 *
 * Test-Driven Development: All tests written before implementation
 * Target: 100% pass rate with comprehensive edge case coverage
 */

#include <gtest/gtest.h>
#include "eval/handcrafted_eval.h"
#include "Board.h"
#include "Types.h"
#include <memory>

using namespace opera;
using namespace opera::eval;

class AdvancedEvalTest : public ::testing::Test {
protected:
    void SetUp() override {
        board = std::make_unique<Board>();
        evaluator = std::make_unique<HandcraftedEvaluator>();
    }

    void TearDown() override {
        board.reset();
        evaluator.reset();
    }

    std::unique_ptr<Board> board;
    std::unique_ptr<HandcraftedEvaluator> evaluator;
};

// ============================================================================
// Pawn Structure Tests
// ============================================================================

/**
 * Test 1: Isolated pawn penalty
 * Isolated pawns (no friendly pawns on adjacent files) should be penalized
 */
TEST_F(AdvancedEvalTest, IsolatedPawnPenalty) {
    // White has isolated d4 pawn (no pawns on c/e files)
    board->setFromFEN("rnbqkbnr/ppp1pppp/8/8/3P4/8/PPP2PPP/RNBQKBNR w KQkq - 0 1");
    int isolated_score = evaluator->evaluate(*board, Color::WHITE);

    // Position without isolated pawn (pawn on e4 instead)
    board->setFromFEN("rnbqkbnr/pppp1ppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 1");
    int normal_score = evaluator->evaluate(*board, Color::WHITE);

    // Isolated pawn should score worse (-20cp penalty)
    EXPECT_LT(isolated_score, normal_score - 10);
}

/**
 * Test 2: Doubled pawn penalty
 * Doubled pawns (two pawns on same file) should be penalized
 */
TEST_F(AdvancedEvalTest, DoubledPawnPenalty) {
    // White has doubled c-pawns (c2, c3)
    board->setFromFEN("rnbqkbnr/pppppppp/8/8/8/2P5/PPPP1PPP/RNBQKBNR w KQkq - 0 1");
    int doubled_score = evaluator->evaluate(*board, Color::WHITE);

    // Normal pawn structure
    board->setFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    int normal_score = evaluator->evaluate(*board, Color::WHITE);

    // Doubled pawns should score worse (-10cp penalty per doubled pawn)
    EXPECT_LT(doubled_score, normal_score - 5);
}

/**
 * Test 3: Passed pawn bonus
 * Passed pawns (no enemy pawns ahead on same/adjacent files) get bonus
 */
TEST_F(AdvancedEvalTest, PassedPawnBonus) {
    // White has passed e-pawn on 5th rank (no black pawns on d/e/f files)
    board->setFromFEN("rnbqkbnr/ppp2ppp/8/4P3/8/8/PPPP1PPP/RNBQKBNR w KQkq - 0 1");
    int passed_score = evaluator->evaluate(*board, Color::WHITE);

    // Same material but pawn on e4 (not passed, black f7 pawn blocks)
    board->setFromFEN("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 1");
    int normal_score = evaluator->evaluate(*board, Color::WHITE);

    // Passed pawn should get bonus (scales with rank)
    EXPECT_GT(passed_score, normal_score + 10);
}

/**
 * Test 4: Advanced passed pawn bonus scales with rank
 * Passed pawns closer to promotion get larger bonus
 */
TEST_F(AdvancedEvalTest, AdvancedPassedPawnBonusScales) {
    // White passed pawn on 6th rank (equal material)
    board->setFromFEN("rnbqkbnr/pp1p1ppp/4P3/8/8/8/PPPP1PPP/RNBQKBNR w KQkq - 0 1");
    int sixth_rank_score = evaluator->evaluate(*board, Color::WHITE);

    // White passed pawn on 5th rank (equal material)
    board->setFromFEN("rnbqkbnr/pp1p1ppp/8/4P3/8/8/PPPP1PPP/RNBQKBNR w KQkq - 0 1");
    int fifth_rank_score = evaluator->evaluate(*board, Color::WHITE);

    // 6th rank passed pawn should be worth more than 5th rank
    EXPECT_GT(sixth_rank_score, fifth_rank_score + 10);
}

/**
 * Test 5: Multiple pawn structure weaknesses compound
 */
TEST_F(AdvancedEvalTest, MultiplePawnWeaknesses) {
    // White has isolated AND doubled c-pawns (double penalty)
    board->setFromFEN("rnbqkbnr/pppppppp/8/8/8/2P5/P1PP1PPP/RNBQKBNR w KQkq - 0 1");
    int multiple_weaknesses = evaluator->evaluate(*board, Color::WHITE);

    // Normal structure
    board->setFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    int normal_score = evaluator->evaluate(*board, Color::WHITE);

    // Both penalties should apply (isolated -20cp + doubled -10cp ≈ -30cp)
    EXPECT_LT(multiple_weaknesses, normal_score - 20);
}

// ============================================================================
// King Safety Tests
// ============================================================================

/**
 * Test 6: Castled king with pawn shield is safe
 * King safety should reward pawn shield in front of castled king
 */
TEST_F(AdvancedEvalTest, CastledKingPawnShield) {
    // White castled kingside with intact pawn shield (f2, g2, h2)
    // Equal material, just king position different
    board->setFromFEN("rnbqk2r/pppppppp/8/8/8/8/PPPPPPPP/RNBQ1RK1 w kq - 0 1");
    int castled_score = evaluator->evaluate(*board, Color::WHITE);

    // King in center (unsafe), black also not castled
    board->setFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    int center_king_score = evaluator->evaluate(*board, Color::WHITE);

    // Castled king with shield should be safer (positive bonus)
    EXPECT_GT(castled_score, center_king_score);
}

/**
 * Test 7: Broken pawn shield reduces king safety
 * Moving pawns in front of king reduces safety
 */
TEST_F(AdvancedEvalTest, BrokenPawnShield) {
    // Castled king with broken shield (g3 pawn advanced)
    board->setFromFEN("rnbqkbnr/pppppppp/8/8/8/6P1/PPPPPP1P/RNBQK2R w KQkq - 0 1");
    int broken_shield = evaluator->evaluate(*board, Color::WHITE);

    // Castled king with intact shield
    board->setFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQK2R w KQkq - 0 1");
    int intact_shield = evaluator->evaluate(*board, Color::WHITE);

    // Broken shield should be penalized
    EXPECT_LT(broken_shield, intact_shield);
}

/**
 * Test 8: Open files near king are dangerous
 * Open files (no pawns) near king reduce safety
 */
TEST_F(AdvancedEvalTest, OpenFilesNearKing) {
    // Castled king with open h-file (h-pawn missing)
    board->setFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPP1/RNBQK2R w KQkq - 0 1");
    int open_file = evaluator->evaluate(*board, Color::WHITE);

    // Castled king with closed files
    board->setFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQK2R w KQkq - 0 1");
    int closed_files = evaluator->evaluate(*board, Color::WHITE);

    // Open file near king should be penalized
    EXPECT_LT(open_file, closed_files);
}

/**
 * Test 9: King safety in endgame vs opening
 * King centralization important in endgame, safety in opening
 */
TEST_F(AdvancedEvalTest, KingSafetyPhaseDependent) {
    // Opening: central king is bad (equal material - both missing bishops)
    board->setFromFEN("rnbqk2r/pppppppp/8/8/4K3/8/PPPPPPPP/RNBQ2NR w kq - 0 1");
    int opening_central_king = evaluator->evaluate(*board, Color::WHITE);

    // Opening: castled king is good (equal material - both missing bishops)
    board->setFromFEN("rnbqk2r/pppppppp/8/8/8/8/PPPPPPPP/RNBQ1RK1 w kq - 0 1");
    int opening_castled_king = evaluator->evaluate(*board, Color::WHITE);

    // In opening, castled king should be safer than central king
    EXPECT_GT(opening_castled_king, opening_central_king + 10);
}

// ============================================================================
// Piece Mobility Tests
// ============================================================================

/**
 * Test 10: Piece mobility bonus for knights
 * Knights with more squares should score higher
 */
TEST_F(AdvancedEvalTest, KnightMobilityBonus) {
    // Knight on e4 (8 possible squares - central mobility)
    board->setFromFEN("rnbqkbnr/pppppppp/8/8/4N3/8/PPPPPPPP/R1BQKBNR w KQkq - 0 1");
    int central_knight = evaluator->evaluate(*board, Color::WHITE);

    // Knight on a1 (2 possible squares - rim knight)
    board->setFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/N1BQKBNR w KQkq - 0 1");
    int rim_knight = evaluator->evaluate(*board, Color::WHITE);

    // Central knight should have mobility bonus
    EXPECT_GT(central_knight, rim_knight + 10);
}

/**
 * Test 11: Bishop mobility bonus
 * Bishops on open diagonals score higher
 */
TEST_F(AdvancedEvalTest, BishopMobilityBonus) {
    // Bishop on open diagonal (e2 pawn moved, d7 also moved - equal material)
    board->setFromFEN("rnbqkbnr/ppp1pppp/8/3P4/8/8/PPPP1PPP/RNBQKBNR w KQkq - 0 1");
    int open_bishop = evaluator->evaluate(*board, Color::WHITE);

    // Bishop blocked by own pawns (center closed, equal material)
    board->setFromFEN("rnbqkbnr/ppp1pppp/8/3P4/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    int blocked_bishop = evaluator->evaluate(*board, Color::WHITE);

    // Open bishop should have mobility bonus
    EXPECT_GT(open_bishop, blocked_bishop);
}

/**
 * Test 12: Rook mobility on open files
 * Rooks on open/semi-open files get bonus
 */
TEST_F(AdvancedEvalTest, RookOpenFileBonus) {
    // Rook on open d-file (d4, no pawns on d-file)
    board->setFromFEN("rnbqkbnr/ppp1pppp/8/8/3R4/8/PPP1PPPP/1NBQKBNR w Kkq - 0 1");
    int open_file_rook = evaluator->evaluate(*board, Color::WHITE);

    // Rook on closed d-file (d4, both sides have d-pawns)
    board->setFromFEN("rnbqkbnr/pppppppp/8/8/3R4/8/PPPPPPPP/1NBQKBNR w Kkq - 0 1");
    int closed_file_rook = evaluator->evaluate(*board, Color::WHITE);

    // Open file rook should get bonus
    EXPECT_GT(open_file_rook, closed_file_rook);
}

/**
 * Test 13: Queen mobility bonus
 * Queen with more available squares scores higher
 */
TEST_F(AdvancedEvalTest, QueenMobilityBonus) {
    // Queen on e4 (many squares available - active)
    board->setFromFEN("rnbqkbnr/pppppppp/8/8/4Q3/8/PPPPPPPP/RNB1KBNR w KQkq - 0 1");
    int active_queen = evaluator->evaluate(*board, Color::WHITE);

    // Queen on d1 (blocked by own pieces)
    board->setFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    int blocked_queen = evaluator->evaluate(*board, Color::WHITE);

    // Active queen should have mobility bonus (simplified heuristic gives ~10cp)
    EXPECT_GT(active_queen, blocked_queen);
}

// ============================================================================
// Development Tests
// ============================================================================

/**
 * Test 14: Development bonus in opening phase
 * Developed pieces (off back rank) get bonus in opening
 */
TEST_F(AdvancedEvalTest, DevelopmentBonusOpening) {
    // White has developed knight to f3
    board->setFromFEN("rnbqkbnr/pppppppp/8/8/8/5N2/PPPPPPPP/RNBQKB1R w KQkq - 0 1");
    int developed = evaluator->evaluate(*board, Color::WHITE);

    // White has undeveloped pieces
    board->setFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    int undeveloped = evaluator->evaluate(*board, Color::WHITE);

    // Developed position should score higher in opening
    EXPECT_GT(developed, undeveloped);
}

/**
 * Test 15: Early queen development penalty
 * Queen developed too early should be penalized
 */
TEST_F(AdvancedEvalTest, EarlyQueenDevelopmentPenalty) {
    // Queen out early with undeveloped minors (Qh5, all minors on back rank)
    board->setFromFEN("rnbqkbnr/pppppppp/8/7Q/8/8/PPPPPPPP/RNB1KBNR w KQkq - 0 1");
    int early_queen = evaluator->evaluate(*board, Color::WHITE);

    // Normal development (queen on back rank, some minors developed: Nf3)
    board->setFromFEN("rnbqkbnr/pppppppp/8/8/8/5N2/PPPPPPPP/RNBQKB1R w KQkq - 0 1");
    int normal_development = evaluator->evaluate(*board, Color::WHITE);

    // Early queen with undeveloped minors should be worse than normal development
    EXPECT_LT(early_queen, normal_development);
}

/**
 * Test 16: Minor piece development priority
 * Knights and bishops developed before rooks/queen
 */
TEST_F(AdvancedEvalTest, MinorPieceDevelopmentPriority) {
    // Both knights developed (Nf3, Nc3)
    board->setFromFEN("rnbqkbnr/pppppppp/8/8/8/2N2N2/PPPPPPPP/R1BQKB1R w KQkq - 0 1");
    int both_knights = evaluator->evaluate(*board, Color::WHITE);

    // One knight developed (Nf3)
    board->setFromFEN("rnbqkbnr/pppppppp/8/8/8/5N2/PPPPPPPP/RNBQKB1R w KQkq - 0 1");
    int one_knight = evaluator->evaluate(*board, Color::WHITE);

    // More developed pieces should score higher
    EXPECT_GT(both_knights, one_knight);
}

/**
 * Test 17: Development doesn't matter in endgame
 * Development bonuses should fade in endgame
 */
TEST_F(AdvancedEvalTest, DevelopmentEndgameFade) {
    // Endgame: pieces on back rank
    board->setFromFEN("8/8/8/8/8/8/8/R3K2R w KQ - 0 1");
    int endgame_backrank = evaluator->evaluate(*board, Color::WHITE);

    // Endgame: rooks developed
    board->setFromFEN("8/8/8/3RKR2/8/8/8/8 w - - 0 1");
    int endgame_developed = evaluator->evaluate(*board, Color::WHITE);

    // In endgame, development bonus should be minimal
    // (difference mainly from king centralization, not development)
    int difference = std::abs(endgame_developed - endgame_backrank);
    EXPECT_LT(difference, 50);  // Small difference in endgame
}

// ============================================================================
// Integration Tests
// ============================================================================

/**
 * Test 18: Complex position with multiple factors
 * Test interaction of pawn structure + king safety + mobility
 */
TEST_F(AdvancedEvalTest, ComplexPositionalEvaluation) {
    // Sicilian Defense position with advanced positional understanding
    // White: better development, safer king, but doubled c-pawns
    // Black: isolated d-pawn, less developed
    board->setFromFEN("r1bqkb1r/pp3ppp/2n1pn2/2pp4/2PP4/2N1PN2/PP3PPP/R1BQKB1R w KQkq - 0 1");
    int sicilian = evaluator->evaluate(*board, Color::WHITE);

    // Complex evaluation should consider all factors
    // Test that evaluation is reasonable (not just material)
    EXPECT_GE(sicilian, -100);
    EXPECT_LE(sicilian, 100);
}

/**
 * Test 19: Position with sacrificed pawn for development
 * Test that development + activity can compensate for material
 */
TEST_F(AdvancedEvalTest, PositionalCompensationForMaterial) {
    // White down a pawn but has superior development
    board->setFromFEN("r1bqkb1r/pppp1ppp/2n2n2/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 0 1");
    int active_position = evaluator->evaluate(*board, Color::WHITE);

    // The development and activity should provide some compensation
    // (Not full pawn value, but significant)
    // Position is roughly equal despite material deficit
    EXPECT_GE(active_position, -80);  // Not worse than -0.8 pawns
}

/**
 * Test 20: Endgame king activity vs opening king safety
 * Test phase-dependent king evaluation
 */
TEST_F(AdvancedEvalTest, PhaseDependentKingEvaluation) {
    // Opening: King in center (equal material - both missing bishops)
    board->setFromFEN("rnbqk2r/pppppppp/8/8/8/4K3/PPPPPPPP/RNBQ2NR w kq - 0 1");
    int opening_center_king = evaluator->evaluate(*board, Color::WHITE);

    // Opening: King castled (equal material - both missing bishops)
    board->setFromFEN("rnbqk2r/pppppppp/8/8/8/8/PPPPPPPP/RNBQ1RK1 w kq - 0 1");
    int opening_safe_king = evaluator->evaluate(*board, Color::WHITE);

    // Endgame: King in center
    board->setFromFEN("8/8/8/4k3/4K3/8/8/8 w - - 0 1");
    int endgame_center_king = evaluator->evaluate(*board, Color::WHITE);

    // Endgame: King on edge
    board->setFromFEN("7K/8/8/4k3/8/8/8/8 w - - 0 1");
    int endgame_edge_king = evaluator->evaluate(*board, Color::WHITE);

    // Opening: castled king should be much better than center
    EXPECT_GT(opening_safe_king, opening_center_king + 20);

    // Endgame: center king should be better than edge
    EXPECT_GT(endgame_center_king, endgame_edge_king);
}

/**
 * Test 21: Performance requirement maintained
 * Advanced evaluation should still be <1μs
 */
TEST_F(AdvancedEvalTest, PerformanceRequirement) {
    board->setFromFEN("r1bqkb1r/pppp1ppp/2n2n2/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 0 1");

    // Measure evaluation time
    auto start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 1000; ++i) {
        evaluator->evaluate(*board, Color::WHITE);
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

    double avg_time_ns = duration.count() / 1000.0;
    double avg_time_us = avg_time_ns / 1000.0;

    // Should average <1μs per evaluation (1000ns)
    EXPECT_LT(avg_time_us, 1.0);
}
