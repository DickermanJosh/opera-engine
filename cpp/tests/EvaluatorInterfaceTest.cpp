/**
 * @file EvaluatorInterfaceTest.cpp
 * @brief Comprehensive tests for the abstract Evaluator interface
 *
 * Tests the strategy pattern evaluator interface contract, ensuring
 * proper abstraction for different evaluation approaches (handcrafted,
 * neural network, Morphy-specialized, etc.).
 */

#include <gtest/gtest.h>
#include "eval/evaluator_interface.h"
#include "Board.h"
#include <memory>
#include <map>
#include <string>

namespace opera {
namespace eval {

/**
 * @brief Mock evaluator implementation for testing interface contract
 *
 * This mock evaluator implements the minimal requirements of the
 * Evaluator interface to validate the abstraction design.
 */
class MockEvaluator : public Evaluator {
private:
    int fixed_score_ = 0;
    bool configure_called_ = false;
    bool on_move_made_called_ = false;
    bool on_move_undone_called_ = false;
    bool on_position_reset_called_ = false;
    std::map<std::string, std::string> last_options_;

public:
    explicit MockEvaluator(int fixed_score = 0) : fixed_score_(fixed_score) {}

    int evaluate(const Board& board, Color side_to_move) override {
        return fixed_score_;
    }

    void configure_options(const std::map<std::string, std::string>& options) override {
        configure_called_ = true;
        last_options_ = options;
    }

    void on_move_made(Move move) override {
        on_move_made_called_ = true;
    }

    void on_move_undone(Move move) override {
        on_move_undone_called_ = true;
    }

    void on_position_reset() override {
        on_position_reset_called_ = true;
    }

    // Test helper methods
    bool was_configure_called() const { return configure_called_; }
    bool was_on_move_made_called() const { return on_move_made_called_; }
    bool was_on_move_undone_called() const { return on_move_undone_called_; }
    bool was_on_position_reset_called() const { return on_position_reset_called_; }
    const std::map<std::string, std::string>& get_last_options() const { return last_options_; }

    void reset_flags() {
        configure_called_ = false;
        on_move_made_called_ = false;
        on_move_undone_called_ = false;
        on_position_reset_called_ = false;
    }
};

/**
 * @brief Mock evaluator that tracks evaluation call count
 */
class CountingEvaluator : public Evaluator {
private:
    mutable int evaluation_count_ = 0;

public:
    int evaluate(const Board& board, Color side_to_move) override {
        ++evaluation_count_;
        return 0;  // Neutral evaluation
    }

    void configure_options(const std::map<std::string, std::string>& options) override {}

    int get_evaluation_count() const { return evaluation_count_; }
    void reset_count() { evaluation_count_ = 0; }
};

class EvaluatorInterfaceTest : public ::testing::Test {
protected:
    void SetUp() override {
        board = std::make_unique<Board>();
        board->setFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    }

    std::unique_ptr<Board> board;
};

// ============================================================================
// Core Interface Contract Tests
// ============================================================================

/**
 * Test 1: Evaluator interface can be instantiated through concrete implementation
 */
TEST_F(EvaluatorInterfaceTest, CanInstantiateConcreteEvaluator) {
    auto evaluator = std::make_unique<MockEvaluator>(100);
    ASSERT_NE(evaluator, nullptr);
}

/**
 * Test 2: Evaluator can be used through base class pointer (polymorphism)
 */
TEST_F(EvaluatorInterfaceTest, PolymorphicUsage) {
    std::unique_ptr<Evaluator> evaluator = std::make_unique<MockEvaluator>(250);

    int score = evaluator->evaluate(*board, Color::WHITE);
    EXPECT_EQ(score, 250);
}

/**
 * Test 3: Evaluate method returns correct score from white's perspective
 */
TEST_F(EvaluatorInterfaceTest, EvaluateFromWhitePerspective) {
    MockEvaluator evaluator(150);

    int score = evaluator.evaluate(*board, Color::WHITE);

    // Score should be positive for white advantage
    EXPECT_EQ(score, 150);
}

/**
 * Test 4: Evaluate method handles black to move correctly
 */
TEST_F(EvaluatorInterfaceTest, EvaluateFromBlackPerspective) {
    board->setFromFEN("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1");

    MockEvaluator evaluator(-50);  // Negative = black advantage

    int score = evaluator.evaluate(*board, Color::BLACK);

    EXPECT_EQ(score, -50);
}

/**
 * Test 5: Configure options method is callable
 */
TEST_F(EvaluatorInterfaceTest, ConfigureOptions) {
    auto evaluator = std::make_unique<MockEvaluator>();

    std::map<std::string, std::string> options = {
        {"Hash", "64"},
        {"MorphyBias", "1.5"}
    };

    evaluator->configure_options(options);

    EXPECT_TRUE(evaluator->was_configure_called());
    EXPECT_EQ(evaluator->get_last_options().at("Hash"), "64");
    EXPECT_EQ(evaluator->get_last_options().at("MorphyBias"), "1.5");
}

/**
 * Test 6: Configure with empty options map doesn't crash
 */
TEST_F(EvaluatorInterfaceTest, ConfigureEmptyOptions) {
    auto evaluator = std::make_unique<MockEvaluator>();

    std::map<std::string, std::string> empty_options;

    EXPECT_NO_THROW(evaluator->configure_options(empty_options));
    EXPECT_TRUE(evaluator->was_configure_called());
}

// ============================================================================
// Incremental Evaluation Hook Tests
// ============================================================================

/**
 * Test 7: on_move_made hook is called when move is made
 */
TEST_F(EvaluatorInterfaceTest, OnMoveMadeHook) {
    auto evaluator = std::make_unique<MockEvaluator>();

    Move test_move(E2, E4, MoveType::NORMAL);

    evaluator->on_move_made(test_move);

    EXPECT_TRUE(evaluator->was_on_move_made_called());
}

/**
 * Test 8: on_move_undone hook is called when move is undone
 */
TEST_F(EvaluatorInterfaceTest, OnMoveUndoneHook) {
    auto evaluator = std::make_unique<MockEvaluator>();

    Move test_move(E2, E4, MoveType::NORMAL);

    evaluator->on_move_undone(test_move);

    EXPECT_TRUE(evaluator->was_on_move_undone_called());
}

/**
 * Test 9: on_position_reset hook is called on position reset
 */
TEST_F(EvaluatorInterfaceTest, OnPositionResetHook) {
    auto evaluator = std::make_unique<MockEvaluator>();

    evaluator->on_position_reset();

    EXPECT_TRUE(evaluator->was_on_position_reset_called());
}

/**
 * Test 10: Incremental hooks are optional (default implementation does nothing)
 */
TEST_F(EvaluatorInterfaceTest, IncrementalHooksOptional) {
    // CountingEvaluator doesn't override incremental hooks
    auto evaluator = std::make_unique<CountingEvaluator>();

    Move test_move(E2, E4, MoveType::NORMAL);

    // These should not crash even if not overridden
    EXPECT_NO_THROW(evaluator->on_move_made(test_move));
    EXPECT_NO_THROW(evaluator->on_move_undone(test_move));
    EXPECT_NO_THROW(evaluator->on_position_reset());
}

// ============================================================================
// Multiple Evaluator Instances Tests
// ============================================================================

/**
 * Test 11: Multiple evaluator instances can coexist
 */
TEST_F(EvaluatorInterfaceTest, MultipleEvaluatorInstances) {
    auto evaluator1 = std::make_unique<MockEvaluator>(100);
    auto evaluator2 = std::make_unique<MockEvaluator>(200);
    auto evaluator3 = std::make_unique<MockEvaluator>(300);

    EXPECT_EQ(evaluator1->evaluate(*board, Color::WHITE), 100);
    EXPECT_EQ(evaluator2->evaluate(*board, Color::WHITE), 200);
    EXPECT_EQ(evaluator3->evaluate(*board, Color::WHITE), 300);
}

/**
 * Test 12: Evaluators are independent (state changes don't affect others)
 */
TEST_F(EvaluatorInterfaceTest, EvaluatorIndependence) {
    auto evaluator1 = std::make_unique<MockEvaluator>();
    auto evaluator2 = std::make_unique<MockEvaluator>();

    Move test_move(E2, E4, MoveType::NORMAL);
    evaluator1->on_move_made(test_move);

    // evaluator2 should not be affected
    EXPECT_TRUE(evaluator1->was_on_move_made_called());
    EXPECT_FALSE(evaluator2->was_on_move_made_called());
}

// ============================================================================
// Interface Performance Tests
// ============================================================================

/**
 * Test 13: Evaluate can be called multiple times on same position
 */
TEST_F(EvaluatorInterfaceTest, MultipleEvaluationsConsistent) {
    auto evaluator = std::make_unique<MockEvaluator>(175);

    int score1 = evaluator->evaluate(*board, Color::WHITE);
    int score2 = evaluator->evaluate(*board, Color::WHITE);
    int score3 = evaluator->evaluate(*board, Color::WHITE);

    EXPECT_EQ(score1, score2);
    EXPECT_EQ(score2, score3);
}

/**
 * Test 14: Evaluation call count tracking works correctly
 */
TEST_F(EvaluatorInterfaceTest, EvaluationCallTracking) {
    auto evaluator = std::make_unique<CountingEvaluator>();

    EXPECT_EQ(evaluator->get_evaluation_count(), 0);

    evaluator->evaluate(*board, Color::WHITE);
    EXPECT_EQ(evaluator->get_evaluation_count(), 1);

    evaluator->evaluate(*board, Color::BLACK);
    EXPECT_EQ(evaluator->get_evaluation_count(), 2);

    evaluator->reset_count();
    EXPECT_EQ(evaluator->get_evaluation_count(), 0);
}

// ============================================================================
// Edge Case and Error Handling Tests
// ============================================================================

/**
 * Test 15: Evaluator handles starting position correctly
 */
TEST_F(EvaluatorInterfaceTest, StartingPositionEvaluation) {
    auto evaluator = std::make_unique<MockEvaluator>(0);

    // Starting position should evaluate to approximately 0 (equal)
    int score = evaluator->evaluate(*board, Color::WHITE);

    EXPECT_EQ(score, 0);
}

/**
 * Test 16: Evaluator handles complex middlegame position
 */
TEST_F(EvaluatorInterfaceTest, MiddlegamePositionEvaluation) {
    board->setFromFEN("r1bqk2r/pppp1ppp/2n2n2/2b1p3/2B1P3/3P1N2/PPP2PPP/RNBQK2R w KQkq - 4 5");

    auto evaluator = std::make_unique<MockEvaluator>(50);

    int score = evaluator->evaluate(*board, Color::WHITE);

    EXPECT_EQ(score, 50);
}

/**
 * Test 17: Evaluator handles endgame position
 */
TEST_F(EvaluatorInterfaceTest, EndgamePositionEvaluation) {
    board->setFromFEN("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1");

    auto evaluator = std::make_unique<MockEvaluator>(-100);

    int score = evaluator->evaluate(*board, Color::WHITE);

    EXPECT_EQ(score, -100);
}

/**
 * Test 18: Configure handles multiple option updates
 */
TEST_F(EvaluatorInterfaceTest, MultipleConfigureCallsWork) {
    auto evaluator = std::make_unique<MockEvaluator>();

    std::map<std::string, std::string> options1 = {{"Hash", "64"}};
    std::map<std::string, std::string> options2 = {{"MorphyBias", "1.5"}};
    std::map<std::string, std::string> options3 = {{"Hash", "128"}, {"MorphyBias", "2.0"}};

    evaluator->configure_options(options1);
    evaluator->configure_options(options2);
    evaluator->configure_options(options3);

    // Last configuration should have both options
    EXPECT_EQ(evaluator->get_last_options().at("Hash"), "128");
    EXPECT_EQ(evaluator->get_last_options().at("MorphyBias"), "2.0");
}

/**
 * Test 19: Incremental hooks can be called in sequence
 */
TEST_F(EvaluatorInterfaceTest, IncrementalHookSequence) {
    auto evaluator = std::make_unique<MockEvaluator>();

    Move move1(E2, E4, MoveType::NORMAL);
    Move move2(E7, E5, MoveType::NORMAL);

    // Make moves
    evaluator->on_move_made(move1);
    evaluator->on_move_made(move2);

    EXPECT_TRUE(evaluator->was_on_move_made_called());

    // Reset flags
    evaluator->reset_flags();

    // Undo moves
    evaluator->on_move_undone(move2);
    evaluator->on_move_undone(move1);

    EXPECT_TRUE(evaluator->was_on_move_undone_called());

    // Reset position
    evaluator->reset_flags();
    evaluator->on_position_reset();

    EXPECT_TRUE(evaluator->was_on_position_reset_called());
}

/**
 * Test 20: Polymorphic evaluator switching works correctly
 */
TEST_F(EvaluatorInterfaceTest, PolymorphicEvaluatorSwitching) {
    std::unique_ptr<Evaluator> current_evaluator;

    // Start with first evaluator
    current_evaluator = std::make_unique<MockEvaluator>(100);
    int score1 = current_evaluator->evaluate(*board, Color::WHITE);
    EXPECT_EQ(score1, 100);

    // Switch to second evaluator
    current_evaluator = std::make_unique<MockEvaluator>(200);
    int score2 = current_evaluator->evaluate(*board, Color::WHITE);
    EXPECT_EQ(score2, 200);

    // Switch to counting evaluator
    current_evaluator = std::make_unique<CountingEvaluator>();
    int score3 = current_evaluator->evaluate(*board, Color::WHITE);
    EXPECT_EQ(score3, 0);
}

} // namespace eval
} // namespace opera
