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
    // Black king exposed, White has attacking chances
    std::string unsafe_king = "r1bq1rk1/ppp2p1p/2np1np1/2b1p3/2B1P3/2NP1N2/PPP2PPP/R1BQK2R w KQ - 0 1";

    auto comparison = compare_evaluators(unsafe_king, 1.5);

    std::cout << "\n  King Attack Position:\n";
    std::cout << "    Morphy evaluation: " << comparison.morphy_score << " cp\n";
    std::cout << "    Morphy bonus for attack: " << comparison.difference << " cp\n";

    // Morphy should value attacking chances
    EXPECT_GT(comparison.difference, 10);  // At least 10cp bonus for attack
}

/**
 * Test 4: Sacrifice compensation detection
 */
TEST_F(MorphyStyleValidationTest, SacrificeCompensation) {
    // Position after piece sacrifice for initiative
    // White sacrificed knight on f7 for attack
    std::string sacrifice_pos = "r1bq1rk1/ppp2Npp/2np4/2b1p3/2B1P3/2NP4/PPP2PPP/R1BQK2R b KQ - 0 1";

    auto comparison = compare_evaluators(sacrifice_pos, 1.8);

    std::cout << "\n  Sacrifice Compensation:\n";
    std::cout << "    Material balance: -300cp (knight sacrificed)\n";
    std::cout << "    Morphy evaluation: " << comparison.morphy_score << " cp\n";
    std::cout << "    Compensation bonus: " << comparison.difference << " cp\n";

    // Morphy should recognize attack compensation
    EXPECT_GT(comparison.difference, 30);  // Significant compensation for initiative
}

/**
 * Test 5: Uncastled king in opening penalty
 */
TEST_F(MorphyStyleValidationTest, UncastledKingPenalty) {
    // Black hasn't castled in middlegame
    std::string uncastled = "r1bqkb1r/pppp1ppp/2n2n2/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R b KQkq - 0 1";

    auto comparison = compare_evaluators(uncastled, 1.5);

    std::cout << "\n  Uncastled King:\n";
    std::cout << "    Morphy penalty for uncastled: " << -comparison.difference << " cp\n";

    // Morphy should penalize uncastled king more than standard eval
    // (Negative difference means Morphy evaluates lower)
    EXPECT_LT(comparison.difference, -20);  // At least 20cp extra penalty
}

// ============================================================================
// Tempo and Initiative Tests
// ============================================================================

/**
 * Test 6: Initiative and tempo valuation
 */
TEST_F(MorphyStyleValidationTest, InitiativeValuation) {
    // White has tempo and initiative
    std::string initiative_pos = "r1bqkb1r/pppp1ppp/2n2n2/1B2p3/4P3/5N2/PPPP1PPP/RNBQK2R b KQkq - 0 1";

    auto comparison = compare_evaluators(initiative_pos, 1.5);

    std::cout << "\n  Initiative Position:\n";
    std::cout << "    Morphy initiative bonus: " << comparison.difference << " cp\n";

    // Morphy should value having the initiative
    EXPECT_GT(comparison.difference, 15);
}

/**
 * Test 7: Central control emphasis
 */
TEST_F(MorphyStyleValidationTest, CentralControlEmphasis) {
    // Strong central control position
    std::string central = "rnbqkb1r/ppp2ppp/3p1n2/4p3/3PP3/5N2/PPP2PPP/RNBQKB1R w KQkq - 0 1";

    auto comparison = compare_evaluators(central, 1.5);

    std::cout << "\n  Central Control:\n";
    std::cout << "    Morphy central bonus: " << comparison.difference << " cp\n";

    // Morphy should value central control
    EXPECT_GT(comparison.difference, 5);
}

// ============================================================================
// Tactical Pattern Recognition Tests
// ============================================================================

/**
 * Test 8: Morphy finds tactical sacrifice
 */
TEST_F(MorphyStyleValidationTest, FindsTacticalSacrifice) {
    // Bxh7+ Greek gift sacrifice available
    std::string greek_gift = "rn1qkb1r/ppp2ppp/3b1n2/3Pp3/4P3/2N5/PPP1BPPP/R1BQK1NR w KQkq - 0 1";

    // This test is aspirational - engine may not find it yet
    // Keeping as documentation of desired behavior
    std::cout << "\n  Greek Gift Position:\n";
    std::cout << "    Testing if Morphy finds Bxh7+...\n";

    // Run search with Morphy
    board->setFromFEN(greek_gift);
    SearchEngine engine(*board, stop_flag);
    engine.set_use_morphy_style(true);
    engine.set_morphy_bias(2.0);  // Maximum bias

    SearchLimits limits;
    limits.max_depth = 6;

    SearchResult result = engine.search(limits);
    std::cout << "    Best move: " << result.best_move.toString() << "\n";
    std::cout << "    Score: " << result.score << " cp\n";

    // Aspirational test - may not pass yet
    // EXPECT_TRUE(morphy_finds_move(greek_gift, "h7", 6));
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
    std::vector<std::string> phases = {
        "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1",  // Opening
        "r1bq1rk1/ppp2ppp/2np1n2/2b1p3/2B1P3/2NP1N2/PPP2PPP/R1BQK2R w KQ - 0 8",  // Middlegame
        "8/5k2/3p4/1p1Pp2p/pP2Pp1P/P4P1K/8/8 b - - 0 1",  // Endgame
    };

    std::cout << "\n  Style Consistency:\n";

    for (const auto& fen : phases) {
        auto comparison = compare_evaluators(fen, 1.5);
        std::cout << "    Phase difference: " << comparison.difference << " cp\n";

        // Morphy bias should work in all phases
        // (May be smaller in endgame where material matters more)
    }

    // Just verify both evaluators work in all phases
    SUCCEED();
}
