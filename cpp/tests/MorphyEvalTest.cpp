/**
 * @file MorphyEvalTest.cpp
 * @brief Tests for Morphy-style evaluator specialization
 *
 * Validates that MorphyEvaluator properly applies Paul Morphy's playing style:
 * - Aggressive king attacks (1.5x king safety weight)
 * - Rapid development (1.2x development weight)
 * - Initiative and tempo (1.1x mobility weight)
 * - Material sacrifice tolerance (100cp compensation)
 * - Uncastled king exploitation (50cp penalty)
 */

#include <gtest/gtest.h>
#include <memory>
#include "Board.h"
#include "eval/morphy_eval.h"

using namespace opera;
using namespace opera::eval;

class MorphyEvalTest : public ::testing::Test {
protected:
    void SetUp() override {
        board = std::make_unique<Board>();
        // Default Morphy bias = 1.0 (standard Morphy style)
        morphy_eval = std::make_unique<MorphyEvaluator>(1.0);
        // Regular evaluator for comparison
        normal_eval = std::make_unique<MorphyEvaluator>(0.0);  // No Morphy bias
    }

    std::unique_ptr<Board> board;
    std::unique_ptr<MorphyEvaluator> morphy_eval;
    std::unique_ptr<MorphyEvaluator> normal_eval;
};

// ============================================================================
// Development Bias Tests (1.2x weight in opening)
// ============================================================================

/**
 * Test 1: Morphy evaluator rewards rapid development more than normal
 */
TEST_F(MorphyEvalTest, DevelopmentBiasInOpening) {
    // Position with good development: Nf3, Bc4 (Italian Game)
    board->setFromFEN("rnbqkb1r/pppp1ppp/5n2/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 0 1");
    int morphy_developed = morphy_eval->evaluate(*board, Color::WHITE);
    int normal_developed = normal_eval->evaluate(*board, Color::WHITE);

    // Morphy should value this position MORE due to development bias
    EXPECT_GT(morphy_developed, normal_developed);

    // Undeveloped position: starting position
    board->setFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    int morphy_start = morphy_eval->evaluate(*board, Color::WHITE);
    int normal_start = normal_eval->evaluate(*board, Color::WHITE);

    // Development bonus difference should be larger for Morphy (1.2x vs 1.0x)
    int morphy_dev_bonus = morphy_developed - morphy_start;
    int normal_dev_bonus = normal_developed - normal_start;
    EXPECT_GT(morphy_dev_bonus, normal_dev_bonus);
}

/**
 * Test 2: Development bias fades in endgame (phase-dependent)
 */
TEST_F(MorphyEvalTest, DevelopmentBiasFadesInEndgame) {
    // Endgame position (few pieces)
    board->setFromFEN("8/8/4k3/8/8/4K3/8/1R6 w - - 0 1");
    int morphy_endgame = morphy_eval->evaluate(*board, Color::WHITE);
    int normal_endgame = normal_eval->evaluate(*board, Color::WHITE);

    // In endgame, development bias shouldn't matter much
    int difference = std::abs(morphy_endgame - normal_endgame);
    EXPECT_LT(difference, 20);  // Less than 20cp difference in endgame
}

// ============================================================================
// King Safety Aggression Tests (1.5x weight for attacks)
// ============================================================================

/**
 * Test 3: Morphy evaluator heavily rewards attacking enemy king
 */
TEST_F(MorphyEvalTest, KingSafetyAggressionBias) {
    // Black king exposed with open files
    board->setFromFEN("rnbq1rk1/ppp2ppp/8/8/8/8/PPPPPPPP/RNBQK2R w KQ - 0 1");
    int morphy_exposed = morphy_eval->evaluate(*board, Color::WHITE);
    int normal_exposed = normal_eval->evaluate(*board, Color::WHITE);

    // Safe king position
    board->setFromFEN("rnbqk2r/pppppppp/8/8/8/8/PPPPPPPP/RNBQK2R w KQkq - 0 1");
    int morphy_safe = morphy_eval->evaluate(*board, Color::WHITE);
    int normal_safe = normal_eval->evaluate(*board, Color::WHITE);

    // Morphy should penalize exposed king MORE (1.5x vs 1.0x)
    int morphy_penalty = morphy_safe - morphy_exposed;
    int normal_penalty = normal_safe - normal_exposed;
    EXPECT_GT(morphy_penalty, normal_penalty);
}

/**
 * Test 4: Uncastled king penalty (Morphy specific)
 */
TEST_F(MorphyEvalTest, UncastledKingPenalty) {
    // Black hasn't castled, white has (both sides have developed minor pieces equally)
    board->setFromFEN("r1bqk2r/pppppppp/2n2n2/8/8/2N2N2/PPPPPPPP/R1BQ1RK1 w kq - 0 1");
    int with_uncastled = morphy_eval->evaluate(*board, Color::WHITE);

    // Both castled (black kingside: ...O-O)
    board->setFromFEN("r1bq1rk1/pppppppp/2n2n2/8/8/2N2N2/PPPPPPPP/R1BQ1RK1 w - - 0 1");
    int both_castled = morphy_eval->evaluate(*board, Color::WHITE);

    // Morphy should heavily penalize uncastled king (target: 50cp)
    int penalty = with_uncastled - both_castled;
    EXPECT_GT(penalty, 40);  // At least 40cp penalty for uncastled enemy king
}

// ============================================================================
// Initiative and Tempo Tests (1.1x mobility weight)
// ============================================================================

/**
 * Test 5: Morphy values piece activity and initiative
 */
TEST_F(MorphyEvalTest, InitiativeAndTempoBias) {
    // Active pieces: knights centralized, rooks on open files
    board->setFromFEN("rnbqkb1r/pppppppp/8/8/3NN3/8/PPPPPPPP/R1BQKB1R w KQkq - 0 1");
    int morphy_active = morphy_eval->evaluate(*board, Color::WHITE);
    int normal_active = normal_eval->evaluate(*board, Color::WHITE);

    // Passive pieces: starting position
    board->setFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    int morphy_passive = morphy_eval->evaluate(*board, Color::WHITE);
    int normal_passive = normal_eval->evaluate(*board, Color::WHITE);

    // Morphy should value active pieces MORE (1.1x vs 1.0x)
    int morphy_activity = morphy_active - morphy_passive;
    int normal_activity = normal_active - normal_passive;
    EXPECT_GT(morphy_activity, normal_activity);
}

// ============================================================================
// Material Sacrifice Compensation Tests
// ============================================================================

/**
 * Test 6: Morphy biases provide natural compensation for positional sacrifices
 */
TEST_F(MorphyEvalTest, MaterialSacrificeCompensation) {
    // Position where white has sacrificed material for overwhelming compensation
    // White has sacrificed the e4 pawn but has massive initiative and development lead
    // White's rooks control open files, pieces are highly active, black king unsafe
    board->setFromFEN("r2qk2r/ppp2ppp/2np1n2/2b1p1B1/1bB5/2NP1N2/PPP2PPP/R2QR1K1 w kq - 0 1");
    int morphy_sacrifice = morphy_eval->evaluate(*board, Color::WHITE);
    int normal_sacrifice = normal_eval->evaluate(*board, Color::WHITE);

    // Morphy's combined biases (development, king safety, mobility) provide compensation
    // Even if material is slightly behind, the positional factors should compensate
    EXPECT_GT(morphy_sacrifice, normal_sacrifice);

    // The compensation from combined biases should be at least 20cp
    EXPECT_GT(morphy_sacrifice - normal_sacrifice, 20);
}

/**
 * Test 7: No compensation for passive material deficits
 */
TEST_F(MorphyEvalTest, NoCompensationForPassiveSacrifice) {
    // Down a knight but no activity/initiative
    board->setFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/R1BQKBNR w KQkq - 0 1");
    int morphy_passive = morphy_eval->evaluate(*board, Color::WHITE);
    int normal_passive = normal_eval->evaluate(*board, Color::WHITE);

    // Without initiative, Morphy shouldn't compensate much
    int difference = std::abs(morphy_passive - normal_passive);
    EXPECT_LT(difference, 30);  // Less than 30cp difference
}

// ============================================================================
// Bias Multiplier Configuration Tests
// ============================================================================

/**
 * Test 8: Bias multiplier scaling (0.0 = normal, 1.0 = standard Morphy, 2.0 = extreme)
 */
TEST_F(MorphyEvalTest, BiasMultiplierScaling) {
    // Create evaluators with different bias levels
    MorphyEvaluator no_bias(0.0);      // Normal play
    MorphyEvaluator standard_bias(1.0); // Standard Morphy
    MorphyEvaluator extreme_bias(2.0);  // Extreme Morphy

    // Position with development advantage
    board->setFromFEN("rnbqkb1r/pppp1ppp/5n2/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 0 1");

    int no_bias_score = no_bias.evaluate(*board, Color::WHITE);
    int standard_score = standard_bias.evaluate(*board, Color::WHITE);
    int extreme_score = extreme_bias.evaluate(*board, Color::WHITE);

    // Scores should increase with bias multiplier
    EXPECT_GT(standard_score, no_bias_score);
    EXPECT_GT(extreme_score, standard_score);
}

/**
 * Test 9: Bias multiplier via UCI configuration
 */
TEST_F(MorphyEvalTest, UCIBiasConfiguration) {
    MorphyEvaluator eval(1.0);

    // Configure via UCI-style options
    std::map<std::string, std::string> options;
    options["MorphyBias"] = "1.5";
    eval.configure_options(options);

    // Bias should now be 1.5
    EXPECT_DOUBLE_EQ(eval.get_morphy_bias(), 1.5);

    // Test with 0.0 (disable Morphy style)
    options["MorphyBias"] = "0.0";
    eval.configure_options(options);
    EXPECT_DOUBLE_EQ(eval.get_morphy_bias(), 0.0);

    // Test with 2.0 (maximum bias)
    options["MorphyBias"] = "2.0";
    eval.configure_options(options);
    EXPECT_DOUBLE_EQ(eval.get_morphy_bias(), 2.0);
}

// ============================================================================
// Integration Tests
// ============================================================================

/**
 * Test 10: Complete Morphy game position evaluation
 */
TEST_F(MorphyEvalTest, MorphyGamePositionEvaluation) {
    // Position with initiative and attacking chances
    // White has sacrificed a pawn for development and king attack
    // White has better development, active pieces, and king pressure
    board->setFromFEN("r1bqk2r/pppp1ppp/2n2n2/2b1p3/1PB1P3/5N2/P1PP1PPP/RNBQ1RK1 w kq - 0 1");

    int morphy_score = morphy_eval->evaluate(*board, Color::WHITE);
    int normal_score = normal_eval->evaluate(*board, Color::WHITE);

    // Morphy evaluator should see this position as better due to:
    // 1. Initiative and activity
    // 2. Development advantage (white castled, black hasn't)
    // 3. Piece mobility (Bc4, Nf3 developed)
    EXPECT_GT(morphy_score, normal_score + 20);  // At least 20cp better from biases
}

/**
 * Test 11: Performance requirement maintained
 */
TEST_F(MorphyEvalTest, PerformanceRequirement) {
    board->setFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");

    // Evaluate 10000 times and measure average time
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 10000; ++i) {
        morphy_eval->evaluate(*board, Color::WHITE);
    }
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    double avg_time_ns = duration.count() / 10000.0;

    // Should maintain <1μs performance (1000ns)
    EXPECT_LT(avg_time_ns, 1000.0);
}

/**
 * Test 12: Morphy vs Normal evaluator comparison
 */
TEST_F(MorphyEvalTest, MorphyVsNormalComparison) {
    // Test multiple positions to ensure consistent bias application
    std::vector<std::string> positions = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",  // Starting
        "rnbqkb1r/pppp1ppp/5n2/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 0 1",  // Italian
        "r1bqkb1r/pppp1ppp/2n2n2/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 0 1",  // Two Knights
    };

    for (const auto& fen : positions) {
        board->setFromFEN(fen);
        int morphy_score = morphy_eval->evaluate(*board, Color::WHITE);
        int normal_score = normal_eval->evaluate(*board, Color::WHITE);

        // Morphy should consistently evaluate developed positions higher
        // (May be equal or higher, never significantly lower)
        EXPECT_GE(morphy_score, normal_score - 20);  // At most 20cp worse
    }
}
