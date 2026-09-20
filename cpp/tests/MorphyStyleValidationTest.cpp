#include <iomanip>
/**
 * @file MorphyStyleValidationTest.cpp
 * @brief Morphy playing style validation tests (Task 4.3)
 *
 * Validates that MorphyEvaluator exhibits Paul Morphy's characteristic style:
 * - Development priority over material
 * - King attack focus and sacrificial play
 * - Initiative and tempo emphasis
 * - Tactical pattern recognition
 *
 * Tests compare MorphyEvaluator vs HandcraftedEvaluator to confirm style differences.
 */

#include <gtest/gtest.h>
#include <memory>
#include <cmath>
#include "Board.h"
#include "search/search_engine.h"
#include "eval/handcrafted_eval.h"
#include "eval/morphy_eval.h"

using namespace opera;
using namespace opera::eval;

class MorphyStyleValidationTest : public ::testing::Test {
protected:
    void SetUp() override {
        board = std::make_unique<Board>();
        stop_flag.store(false);
    }

    /**
     * Compare evaluations between Morphy and Handcrafted evaluators
     */
    struct EvalComparison {
        int morphy_score;
        int handcrafted_score;
        int difference;
        bool morphy_prefers;  // True if Morphy rates position higher
    };

    EvalComparison compare_evaluators(const std::string& fen, double morphy_bias = 1.2) {
        board->setFromFEN(fen);

        HandcraftedEvaluator handcrafted;
        MorphyEvaluator morphy(morphy_bias);

        Color side = board->getSideToMove();

        EvalComparison result;
        result.handcrafted_score = handcrafted.evaluate(*board, side);
        result.morphy_score = morphy.evaluate(*board, side);
        result.difference = result.morphy_score - result.handcrafted_score;
        result.morphy_prefers = result.difference > 0;

        return result;
    }

    /**
     * Test if engine with Morphy bias finds specific move
     */
    bool morphy_finds_move(const std::string& fen, const std::string& expected_move,
                          int max_depth = 6) {
        board->setFromFEN(fen);
        SearchEngine engine(*board, stop_flag);

        engine.set_use_morphy_style(true);
        engine.set_morphy_bias(1.5);  // Strong Morphy bias

        SearchLimits limits;
        limits.max_depth = max_depth;

        SearchResult result = engine.search(limits);
        std::string move_str = result.best_move.toString();

        return move_str.find(expected_move) != std::string::npos;
    }

    std::unique_ptr<Board> board;
    std::atomic<bool> stop_flag;
};

// ============================================================================
// Development Priority Tests
// ============================================================================

/**
 * Test 1: Morphy values piece development more than material
 */
TEST_F(MorphyStyleValidationTest, DevelopmentOverMaterial) {
    // Position: White has developed pieces but is down a pawn
    // Morphy should value this more positively than material-focused eval
    std::string fen = "r1bqkb1r/pppp1ppp/2n2n2/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 0 1";

    auto comparison = compare_evaluators(fen, 1.5);

    std::cout << "\n  Development vs Material:\n";
    std::cout << "    Handcrafted: " << comparison.handcrafted_score << " cp\n";
    std::cout << "    Morphy:      " << comparison.morphy_score << " cp\n";
    std::cout << "    Difference:  " << comparison.difference << " cp\n";

    // Morphy should value developed position despite material equality
    // At minimum, should not penalize development positions as heavily
    EXPECT_TRUE(comparison.difference >= -50);  // Not too negative
}

/**
 * Test 2: Early development bonus
 */
TEST_F(MorphyStyleValidationTest, EarlyDevelopmentBonus) {
    // Both sides equal material, White has better development
    std::string developed = "rnbqkb1r/pppp1ppp/5n2/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 0 1";
    std::string undeveloped = "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1";

    auto dev_comp = compare_evaluators(developed, 1.5);
    auto undev_comp = compare_evaluators(undeveloped, 1.5);

    std::cout << "\n  Development Bonus:\n";
    std::cout << "    Developed position Morphy boost:   " << dev_comp.difference << " cp\n";
    std::cout << "    Undeveloped position Morphy boost: " << undev_comp.difference << " cp\n";

    // Morphy should give bigger bonus to developed position
    EXPECT_GT(dev_comp.difference, undev_comp.difference);
}

// ============================================================================
// King Attack and Sacrificial Play Tests
// ============================================================================

/**
 * Test 3: King safety differential emphasis
 */
TEST_F(MorphyStyleValidationTest, KingSafetyEmphasis) {
    // Equal material, White sheltered on g1; Black has walked to e6.
    const auto result = compare_evaluators("r2q1b1r/ppp2ppp/2nbkn2/3pp3/3PP3/2NB1N2/PPP2PPP/R1BQ1RK1 w - - 0 8", 1.5);
    EXPECT_GT(result.difference, 10);
}

/**
 * Test 4: Sacrifice compensation detection
 */
TEST_F(MorphyStyleValidationTest, SacrificeCompensation) {
    // A pawn deficit with development and king safety, versus a passive pawn loss.
    const auto active = compare_evaluators("rnbqkbnr/pppp1ppp/8/4p3/2B1P3/5N2/PPPP1PP1/RNBQ1RK1 b kq - 4 3", 1.5);
    const auto passive = compare_evaluators("rnbqkbnr/pppppppp/8/8/8/8/1PPPPPPP/RNBQKBNR w KQkq - 0 1", 1.5);
    EXPECT_GT(active.difference, passive.difference);
    EXPECT_LT(passive.morphy_score, 0); // Losing material alone earns no compensation.
}

/**
 * Test 5: Uncastled king in opening penalty
 */
TEST_F(MorphyStyleValidationTest, UncastledKingPenalty) {
    // Scores are White-relative, including when it is Black's turn.
    const auto white_safe = compare_evaluators("rnbqkbnr/pppp1ppp/8/4p3/2B1P3/5N2/PPPP1PPP/RNBQ1RK1 b kq - 4 3", 1.5);
    const auto black_safe = compare_evaluators("rnbq1rk1/pppp1ppp/5n2/2b1p3/4P3/8/PPPP1PPP/RNBQKBNR w KQ - 4 3", 1.5);
    EXPECT_GT(white_safe.difference, 20);
    EXPECT_EQ(white_safe.difference, -black_safe.difference);
}

// ============================================================================
// Tempo and Initiative Tests
// ============================================================================

/**
 * Test 6: Initiative and tempo valuation
 */
TEST_F(MorphyStyleValidationTest, InitiativeValuation) {
    // Genuine development lead with equal material (after a legal opening).
    const auto active = compare_evaluators("rnbqkbnr/pppp1ppp/8/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R b KQkq - 3 3", 1.5);
    const auto symmetric = compare_evaluators(STARTING_FEN, 1.5);
    EXPECT_GT(active.difference, symmetric.difference);
}

/**
 * Test 7: Central control emphasis
 */
TEST_F(MorphyStyleValidationTest, CentralControlEmphasis) {
    // Isolate the central-activity term with equally sheltered kings and pawns.
    const auto central = compare_evaluators("r2q1rk1/ppp2ppp/2nbbn2/8/3N4/3BBN2/PPP2PPP/R2Q1RK1 w - - 0 1", 1.5);
    const auto rim = compare_evaluators("r2q1rk1/ppp2ppp/2nbbn2/8/N7/3BBN2/PPP2PPP/R2Q1RK1 w - - 0 1", 1.5);
    EXPECT_GT(central.difference, rim.difference);
}

// ============================================================================
// Tactical Pattern Recognition Tests
// ============================================================================

/**
 * Test 8: Morphy finds tactical sacrifice
 */
TEST_F(MorphyStyleValidationTest, FindsTacticalSacrifice) {
    // Morphy's Opera Game: Qb8+! Nxb8 Rd8#.
    EXPECT_TRUE(morphy_finds_move("4kb1r/p2n1ppp/4q3/4p1B1/4P3/1Q6/PPP2PPP/2KR4 w k - 0 16", "b3b8", 4));
}

/**
 * Test 9: Aggressive move preference
 */
TEST_F(MorphyStyleValidationTest, AggressiveMovePreference) {
    // Position where aggressive and passive moves available
    std::string choice = "r1bqkb1r/pppp1ppp/2n2n2/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 0 1";

    // Search with Morphy
    board->setFromFEN(choice);
    SearchEngine morphy_engine(*board, stop_flag);
    morphy_engine.set_use_morphy_style(true);
    morphy_engine.set_morphy_bias(1.5);

    SearchLimits limits;
    limits.max_depth = 4;

    SearchResult morphy_result = morphy_engine.search(limits);

    // Search with standard eval
    stop_flag.store(false);
    SearchEngine standard_engine(*board, stop_flag);
    standard_engine.set_use_morphy_style(false);

    SearchResult standard_result = standard_engine.search(limits);

    std::cout << "\n  Aggressive vs Passive:\n";
    std::cout << "    Morphy move:    " << morphy_result.best_move.toString() << "\n";
    std::cout << "    Standard move:  " << standard_result.best_move.toString() << "\n";

    // Moves may differ (demonstrating style difference)
    // Not asserting specific move, just documenting behavior
}

// ============================================================================
// Bias Configuration Tests
// ============================================================================

/**
 * Test 10: Bias scaling validation
 */
TEST_F(MorphyStyleValidationTest, BiasScaling) {
    std::string test_pos = "r1bqkb1r/pppp1ppp/2n2n2/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 0 1";

    // Test multiple bias levels
    auto bias_0 = compare_evaluators(test_pos, 0.0);
    auto bias_1 = compare_evaluators(test_pos, 1.0);
    auto bias_2 = compare_evaluators(test_pos, 2.0);

    std::cout << "\n  Bias Scaling:\n";
    std::cout << "    Bias 0.0: " << bias_0.morphy_score << " cp (diff: " << bias_0.difference << ")\n";
    std::cout << "    Bias 1.0: " << bias_1.morphy_score << " cp (diff: " << bias_1.difference << ")\n";
    std::cout << "    Bias 2.0: " << bias_2.morphy_score << " cp (diff: " << bias_2.difference << ")\n";

    // Higher bias should increase Morphy-style bonuses
    EXPECT_LT(bias_0.difference, bias_1.difference);
    EXPECT_LT(bias_1.difference, bias_2.difference);
}

/**
 * Test 11: UCI bias configuration
 */
TEST_F(MorphyStyleValidationTest, UCIBiasConfiguration) {
    board->setFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    SearchEngine engine(*board, stop_flag);

    // Configure via UCI options
    engine.set_use_morphy_style(true);
    engine.set_morphy_bias(1.5);

    SearchLimits limits;
    limits.max_depth = 2;

    SearchResult result = engine.search(limits);

    std::cout << "\n  UCI Configuration:\n";
    std::cout << "    Morphy bias: 1.5\n";
    std::cout << "    Search completed successfully\n";
    std::cout << "    Best move: " << result.best_move.toString() << "\n";

    EXPECT_GT(result.nodes, 0);
    EXPECT_NE(result.best_move.from(), result.best_move.to());
}

// ============================================================================
// Performance Comparison Tests
// ============================================================================

/**
 * Test 12: Evaluation speed comparison
 */
TEST_F(MorphyStyleValidationTest, EvaluationSpeedComparison) {
    board->setFromFEN("r1bqkb1r/pppp1ppp/2n2n2/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 0 1");

    HandcraftedEvaluator handcrafted;
    MorphyEvaluator morphy(1.2);

    constexpr int iterations = 10000;
    Color side = WHITE;

    // Time handcrafted
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        handcrafted.evaluate(*board, side);
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto handcrafted_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Time Morphy
    start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < iterations; ++i) {
        morphy.evaluate(*board, side);
    }
    end = std::chrono::high_resolution_clock::now();
    auto morphy_time = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    double handcrafted_avg = static_cast<double>(handcrafted_time.count()) / iterations;
    double morphy_avg = static_cast<double>(morphy_time.count()) / iterations;
    double overhead = ((morphy_avg - handcrafted_avg) / handcrafted_avg) * 100.0;

    std::cout << "\n  Evaluation Speed:\n";
    std::cout << "    Handcrafted: " << std::fixed << std::setprecision(3) << handcrafted_avg << " μs/eval\n";
    std::cout << "    Morphy:      " << std::fixed << std::setprecision(3) << morphy_avg << " μs/eval\n";
    std::cout << "    Overhead:    " << std::fixed << std::setprecision(1) << overhead << "%\n";

    // Both should be under 1μs
    EXPECT_LT(handcrafted_avg, 1.0);
    EXPECT_LT(morphy_avg, 1.5);  // Allow slight overhead for Morphy features
}

/**
 * Test 13: Style consistency across game phases
 */
TEST_F(MorphyStyleValidationTest, StyleConsistencyAcrossPhases) {
    for (const auto* fen : {STARTING_FEN,
         "r2q1b1r/ppp2ppp/2nbkn2/3pp3/3PP3/2NB1N2/PPP2PPP/R1BQ1RK1 w - - 0 8",
         "8/5k2/3p4/1p1Pp2p/pP2Pp1P/P4P1K/8/8 b - - 0 1"}) {
        const auto zero = compare_evaluators(fen, 0.0);
        EXPECT_EQ(zero.difference, 0);
        const auto biased = compare_evaluators(fen, 1.5);
        EXPECT_LT(std::abs(biased.difference), 400);
    }
}
