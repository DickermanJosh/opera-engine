#include <gtest/gtest.h>
#include "search/search_engine.h"
#include "Board.h"

using namespace opera;

class UCIOptionsTest : public ::testing::Test {
protected:
    void SetUp() override {
        board = std::make_unique<Board>();
        stop_flag.store(false);
        engine = std::make_unique<SearchEngine>(*board, stop_flag);
    }

    std::unique_ptr<Board> board;
    std::atomic<bool> stop_flag{false};
    std::unique_ptr<SearchEngine> engine;
};

TEST_F(UCIOptionsTest, DefaultParameterValues) {
    EXPECT_EQ(engine->get_null_move_reduction(), DEFAULT_NULL_MOVE_REDUCTION);
    EXPECT_EQ(engine->get_lmr_full_depth_moves(), DEFAULT_LMR_FULL_DEPTH_MOVES);
    EXPECT_EQ(engine->get_lmr_reduction_limit(), DEFAULT_LMR_REDUCTION_LIMIT);
    EXPECT_EQ(engine->get_futility_margin(), DEFAULT_FUTILITY_MARGIN);
    EXPECT_EQ(engine->get_razoring_margin(), DEFAULT_RAZORING_MARGIN);
    EXPECT_EQ(engine->get_min_depth_for_nmp(), DEFAULT_MIN_DEPTH_FOR_NMP);
    EXPECT_EQ(engine->get_min_depth_for_lmr(), DEFAULT_MIN_DEPTH_FOR_LMR);
    EXPECT_EQ(engine->get_min_depth_for_futility(), DEFAULT_MIN_DEPTH_FOR_FUTILITY);
    EXPECT_EQ(engine->get_min_depth_for_razoring(), DEFAULT_MIN_DEPTH_FOR_RAZORING);
}

TEST_F(UCIOptionsTest, SetAndGetNullMoveReduction) {
    engine->set_null_move_reduction(2);
    EXPECT_EQ(engine->get_null_move_reduction(), 2);
    
    engine->set_null_move_reduction(4);
    EXPECT_EQ(engine->get_null_move_reduction(), 4);
}

TEST_F(UCIOptionsTest, SetAndGetLMRParameters) {
    engine->set_lmr_full_depth_moves(3);
    EXPECT_EQ(engine->get_lmr_full_depth_moves(), 3);
    
    engine->set_lmr_reduction_limit(2);
    EXPECT_EQ(engine->get_lmr_reduction_limit(), 2);
    
    engine->set_min_depth_for_lmr(1);
    EXPECT_EQ(engine->get_min_depth_for_lmr(), 1);
}

TEST_F(UCIOptionsTest, SetAndGetPruningParameters) {
    engine->set_futility_margin(150);
    EXPECT_EQ(engine->get_futility_margin(), 150);
    
    engine->set_razoring_margin(250);
    EXPECT_EQ(engine->get_razoring_margin(), 250);
    
    engine->set_min_depth_for_futility(2);
    EXPECT_EQ(engine->get_min_depth_for_futility(), 2);
    
    engine->set_min_depth_for_razoring(3);
    EXPECT_EQ(engine->get_min_depth_for_razoring(), 3);
}

TEST_F(UCIOptionsTest, ParameterRangeValidation) {
    // Test null move reduction
    engine->set_null_move_reduction(1);
    EXPECT_EQ(engine->get_null_move_reduction(), 1);
    
    engine->set_null_move_reduction(5);
    EXPECT_EQ(engine->get_null_move_reduction(), 5);
    
    // Test LMR parameters
    engine->set_lmr_full_depth_moves(1);
    EXPECT_EQ(engine->get_lmr_full_depth_moves(), 1);
    
    engine->set_lmr_reduction_limit(1);
    EXPECT_EQ(engine->get_lmr_reduction_limit(), 1);
}

TEST_F(UCIOptionsTest, SearchBehaviorWithModifiedParameters) {
    // Set custom parameters for a shallow search
    engine->set_min_depth_for_lmr(1);     // Enable LMR at depth 1
    engine->set_lmr_full_depth_moves(1);  // Only first move gets full depth
    engine->set_lmr_reduction_limit(1);   // Limit reduction to 1 ply
    
    // Perform shallow search to test parameter effectiveness
    SearchLimits limits;
    limits.max_depth = 4;
    limits.max_time_ms = 1000;
    
    SearchResult result = engine->search(limits);
    
    // Verify search completes successfully
    EXPECT_GT(result.nodes, 0);
    EXPECT_GT(result.depth, 0);
    EXPECT_NE(result.best_move, Move());
}