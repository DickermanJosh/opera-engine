#include <gtest/gtest.h>
#include <chrono>
#include <thread>
#include "search/search_engine.h"
#include "Board.h"

using namespace opera;

class SearchControlTest : public ::testing::Test {
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

TEST_F(SearchControlTest, AspirationWindowsBasicOperation) {
    SearchLimits limits;
    limits.max_depth = 4;
    limits.max_time_ms = 1000;

    auto start = std::chrono::high_resolution_clock::now();
    SearchResult result = engine->search(limits);
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should complete successfully with aspiration windows
    EXPECT_GT(result.nodes, 0);
    EXPECT_GT(result.depth, 0);
    EXPECT_NE(result.best_move, Move());
    
    // Should respect time limit
    EXPECT_LE(duration.count(), 1100);  // Allow small buffer for overhead
}

TEST_F(SearchControlTest, SearchInfoUpdating) {
    SearchLimits limits;
    limits.max_depth = 3;
    limits.max_time_ms = 500;

    SearchResult result = engine->search(limits);
    const SearchInfo& info = engine->get_search_info();

    // Search info should be populated after search
    EXPECT_EQ(info.depth, result.depth);
    EXPECT_EQ(info.score, result.score);
    EXPECT_EQ(info.nodes, result.nodes);
    EXPECT_GT(info.time_ms, 0);
    EXPECT_GT(info.nps, 0);
    EXPECT_FALSE(info.pv.empty());
}

TEST_F(SearchControlTest, EmergencyStopHandling) {
    SearchLimits limits;
    limits.max_depth = 10;
    limits.max_time_ms = 2000;  // 2 second limit

    // Start search in separate thread
    std::atomic<bool> search_completed{false};
    SearchResult result;
    
    std::thread search_thread([&]() {
        result = engine->search(limits);
        search_completed.store(true);
    });

    // Let search run for a bit
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Trigger emergency stop
    auto stop_time = std::chrono::high_resolution_clock::now();
    stop_flag.store(true);
    
    // Wait for search to complete
    search_thread.join();
    auto complete_time = std::chrono::high_resolution_clock::now();
    
    auto stop_response = std::chrono::duration_cast<std::chrono::milliseconds>(
        complete_time - stop_time);

    // Should stop within reasonable time (target: <10ms, allow <50ms for test stability)
    EXPECT_LT(stop_response.count(), 50);
    EXPECT_TRUE(search_completed.load());
    
    // Should still have valid results up to the point of stopping
    EXPECT_GT(result.nodes, 0);
    EXPECT_GT(result.depth, 0);
}

TEST_F(SearchControlTest, HardTimeLimitRespected) {
    SearchLimits limits;
    limits.max_depth = 20;  // Deep search
    limits.max_time_ms = 200;  // Short time limit

    auto start = std::chrono::high_resolution_clock::now();
    SearchResult result = engine->search(limits);
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

    // Should respect time limit (with emergency buffer)
    EXPECT_LE(duration.count(), 220);  // Allow 20ms buffer
    
    // Should still produce valid results
    EXPECT_GT(result.nodes, 0);
    EXPECT_GT(result.depth, 0);
    EXPECT_NE(result.best_move, Move());
}

TEST_F(SearchControlTest, SearchStartupTime) {
    SearchLimits limits;
    limits.max_depth = 1;  // Minimal search
    limits.max_time_ms = 100;

    auto start = std::chrono::high_resolution_clock::now();
    SearchResult result = engine->search(limits);
    auto end = std::chrono::high_resolution_clock::now();
    
    auto total_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    // Startup should be very fast for shallow search
    // Target: <1ms startup, but allow reasonable overhead
    EXPECT_LT(total_duration.count(), 10000);  // Less than 10ms for depth-1 search
    
    // Should still produce valid results
    EXPECT_GT(result.nodes, 0);
    EXPECT_EQ(result.depth, 1);
    EXPECT_NE(result.best_move, Move());
}

TEST_F(SearchControlTest, DepthCompletionGuarantee) {
    SearchLimits limits;
    limits.max_depth = 3;
    limits.max_time_ms = 100;  // Tight time limit

    SearchResult result = engine->search(limits);

    // Should complete at least depth 1, preferably target depth
    EXPECT_GE(result.depth, 1);
    EXPECT_GT(result.nodes, 0);
    EXPECT_NE(result.best_move, Move());
    
    // If we got to depth 3, it should be a complete search at that depth
    if (result.depth >= 3) {
        EXPECT_GT(result.nodes, 100);  // Should have searched substantial nodes
    }
}

TEST_F(SearchControlTest, NodesLimitRespected) {
    SearchLimits limits;
    limits.max_depth = 20;
    limits.max_nodes = 1000;  // Node limit
    limits.max_time_ms = 5000;  // Generous time limit

    SearchResult result = engine->search(limits);

    // Should respect node limit (allow some overflow due to bulk counting)
    EXPECT_LE(result.nodes, 1200);  // Allow 20% overflow
    EXPECT_GT(result.nodes, 800);   // Should get close to limit
    
    EXPECT_GT(result.depth, 0);
    EXPECT_NE(result.best_move, Move());
}

TEST_F(SearchControlTest, InfiniteSearchHandling) {
    SearchLimits limits;
    limits.max_depth = 20;
    limits.infinite = true;  // Infinite search mode
    limits.max_time_ms = UINT64_MAX;

    // Start infinite search in separate thread
    std::atomic<bool> search_started{false};
    SearchResult result;
    
    std::thread search_thread([&]() {
        search_started.store(true);
        result = engine->search(limits);
    });

    // Wait for search to start
    while (!search_started.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    
    // Let it run briefly
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // Stop the infinite search
    stop_flag.store(true);
    search_thread.join();

    // Should have searched multiple depths
    EXPECT_GT(result.depth, 2);
    EXPECT_GT(result.nodes, 100);
    EXPECT_NE(result.best_move, Move());
}

TEST_F(SearchControlTest, CheckmateEarlyTermination) {
    // Set up a position close to checkmate for early termination testing
    // This is a simplified test - in practice, we'd need a specific position
    SearchLimits limits;
    limits.max_depth = 10;
    limits.max_time_ms = 1000;

    SearchResult result = engine->search(limits);

    // Basic validation - actual checkmate detection would require specific positions
    EXPECT_GT(result.nodes, 0);
    EXPECT_GT(result.depth, 0);
    EXPECT_NE(result.best_move, Move());
    
    // If a checkmate is found (score > 29000), search should terminate early
    if (abs(result.score) > 29000) {
        // Early termination should have occurred
        EXPECT_LT(result.depth, limits.max_depth);
    }
}