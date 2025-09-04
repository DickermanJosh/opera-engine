#include <gtest/gtest.h>
#include <memory>
#include <atomic>
#include <chrono>
#include <thread>
#include "search/search_engine.h"
#include "Board.h"

using namespace opera;

class SearchEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        board = std::make_unique<Board>();
        stop_flag = std::make_unique<std::atomic<bool>>(false);
        search_engine = std::make_unique<SearchEngine>(*board, *stop_flag);
    }

    void TearDown() override {
        stop_flag->store(false);  // Ensure clean state
    }

    std::unique_ptr<Board> board;
    std::unique_ptr<std::atomic<bool>> stop_flag;
    std::unique_ptr<SearchEngine> search_engine;
};

// Basic Construction and Interface Tests
TEST_F(SearchEngineTest, DefaultConstruction) {
    EXPECT_FALSE(search_engine->is_searching());
    EXPECT_EQ(search_engine->get_nodes_searched(), 0);
}

TEST_F(SearchEngineTest, SearchLimitsConstructor) {
    SearchLimits limits;
    limits.max_depth = 5;
    limits.max_time_ms = 1000;
    limits.max_nodes = 50000;
    
    EXPECT_EQ(limits.max_depth, 5);
    EXPECT_EQ(limits.max_time_ms, 1000);
    EXPECT_EQ(limits.max_nodes, 50000);
    EXPECT_FALSE(limits.infinite);
}

// Search Functionality Tests
TEST_F(SearchEngineTest, BasicSearchFromStartingPosition) {
    board->setFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    
    SearchLimits limits;
    limits.max_depth = 3;
    limits.max_time_ms = 5000;  // Generous time limit
    
    auto result = search_engine->search(limits);
    
    EXPECT_NE(result.best_move, NULL_MOVE);
    EXPECT_GT(result.depth, 0);
    EXPECT_LE(result.depth, 3);  // Should not exceed max depth
    EXPECT_GT(result.nodes, 0);
    EXPECT_GE(result.time_ms, 0);
    EXPECT_FALSE(search_engine->is_searching());  // Should be done after search
}

TEST_F(SearchEngineTest, SearchRespectsDepthlimit) {
    board->setFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    
    SearchLimits limits;
    limits.max_depth = 2;
    limits.max_time_ms = 10000;  // Plenty of time
    
    auto result = search_engine->search(limits);
    
    EXPECT_LE(result.depth, 2);  // Must respect depth limit
    EXPECT_GT(result.nodes, 20);  // Should search meaningful nodes at depth 2
}

TEST_F(SearchEngineTest, SearchRespectsTimeLimit) {
    board->setFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    
    SearchLimits limits;
    limits.max_depth = 10;  // High depth
    limits.max_time_ms = 100;  // Short time limit
    
    auto start_time = std::chrono::high_resolution_clock::now();
    auto result = search_engine->search(limits);
    auto end_time = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    EXPECT_LE(duration.count(), 150);  // Should stop within reasonable time of limit
    EXPECT_NE(result.best_move, NULL_MOVE);
}

TEST_F(SearchEngineTest, SearchRespectsNodeLimit) {
    board->setFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    
    SearchLimits limits;
    limits.max_depth = 10;  // High depth
    limits.max_nodes = 1000;  // Limit nodes
    limits.max_time_ms = 10000;  // Plenty of time
    
    auto result = search_engine->search(limits);
    
    EXPECT_LE(result.nodes, 1200);  // Allow some overshoot due to search structure
    EXPECT_NE(result.best_move, NULL_MOVE);
}

// Iterative Deepening Tests
TEST_F(SearchEngineTest, IterativeDeepeningProgression) {
    board->setFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    
    SearchLimits limits;
    limits.max_depth = 4;
    
    auto result = search_engine->search(limits);
    
    // Should complete multiple depths
    EXPECT_GT(result.depth, 1);
    EXPECT_LE(result.depth, 4);
    
    // Should have searched significant nodes for iterative deepening
    EXPECT_GT(result.nodes, 100);
}

// Async Stop Flag Tests
TEST_F(SearchEngineTest, StopFlagTerminatesSearch) {
    board->setFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    
    SearchLimits limits;
    limits.max_depth = 10;  // Deep search
    limits.max_time_ms = 30000;  // Long time
    
    // Start search in background
    std::atomic<bool> search_completed(false);
    SearchResult result;
    
    std::thread search_thread([&]() {
        result = search_engine->search(limits);
        search_completed.store(true);
    });
    
    // Let search run for a bit
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // Set stop flag
    stop_flag->store(true);
    
    // Wait for search to complete
    search_thread.join();
    
    EXPECT_TRUE(search_completed.load());
    EXPECT_NE(result.best_move, NULL_MOVE);  // Should still return a move
    EXPECT_FALSE(search_engine->is_searching());
}

TEST_F(SearchEngineTest, StopMethodTerminatesSearch) {
    board->setFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    
    SearchLimits limits;
    limits.max_depth = 10;
    limits.max_time_ms = 30000;
    
    std::atomic<bool> search_completed(false);
    SearchResult result;
    
    std::thread search_thread([&]() {
        result = search_engine->search(limits);
        search_completed.store(true);
    });
    
    // Let search start
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
    // Call stop method
    search_engine->stop();
    
    search_thread.join();
    
    EXPECT_TRUE(search_completed.load());
    EXPECT_NE(result.best_move, NULL_MOVE);
    EXPECT_FALSE(search_engine->is_searching());
}

// Search Info and Progress Tests
TEST_F(SearchEngineTest, SearchInfoUpdates) {
    board->setFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    
    SearchLimits limits;
    limits.max_depth = 4;
    
    auto result = search_engine->search(limits);
    
    auto info = search_engine->get_search_info();
    
    EXPECT_EQ(info.depth, result.depth);
    EXPECT_EQ(info.nodes, result.nodes);
    EXPECT_GT(info.time_ms, 0);
    EXPECT_NE(info.pv, "");  // Should have principal variation
}

// Edge Cases and Error Handling
TEST_F(SearchEngineTest, CheckmatePosition) {
    // Fool's mate position - black to move, checkmate in 1
    board->setFromFEN("rnb1kbnr/pppp1ppp/8/4p3/6Pq/5P2/PPPPP2P/RNBQKBNR w KQkq - 1 3");
    
    SearchLimits limits;
    limits.max_depth = 3;
    
    auto result = search_engine->search(limits);
    
    // Should handle checkmate gracefully
    EXPECT_NE(result.best_move, NULL_MOVE);  // Should find a legal move
    EXPECT_GT(result.depth, 0);
}

TEST_F(SearchEngineTest, StalematePosition) {
    // Stalemate position
    board->setFromFEN("8/8/8/8/8/6k1/5q2/7K b - - 0 1");
    
    SearchLimits limits;
    limits.max_depth = 3;
    
    auto result = search_engine->search(limits);
    
    // Should handle stalemate gracefully
    EXPECT_GT(result.depth, 0);
    // Note: In stalemate, there might not be a legal move, so best_move could be NULL_MOVE
}

TEST_F(SearchEngineTest, InfiniteSearchMode) {
    board->setFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    
    SearchLimits limits;
    limits.infinite = true;
    limits.max_depth = 3;  // Still limit depth for testing
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Start infinite search in background
    std::atomic<bool> search_completed(false);
    SearchResult result;
    
    std::thread search_thread([&]() {
        result = search_engine->search(limits);
        search_completed.store(true);
    });
    
    // Let it run briefly
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // Stop the infinite search
    search_engine->stop();
    search_thread.join();
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    EXPECT_TRUE(search_completed.load());
    EXPECT_NE(result.best_move, NULL_MOVE);
    EXPECT_GT(duration.count(), 90);  // Should have run for a reasonable time
}

// Performance Tests
TEST_F(SearchEngineTest, SearchStartupTime) {
    board->setFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    
    SearchLimits limits;
    limits.max_depth = 1;  // Minimal search for startup timing
    
    auto start_time = std::chrono::high_resolution_clock::now();
    auto result = search_engine->search(limits);
    auto end_time = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    // Search startup should be under 1ms (1000 microseconds)
    EXPECT_LT(duration.count(), 1000);
    EXPECT_NE(result.best_move, NULL_MOVE);
}

TEST_F(SearchEngineTest, MinimalNodePerformance) {
    board->setFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    
    SearchLimits limits;
    limits.max_depth = 4;
    limits.max_time_ms = 5000;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    auto result = search_engine->search(limits);
    auto end_time = std::chrono::high_resolution_clock::now();
    
    auto duration_ms = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    if (duration_ms.count() > 0) {
        uint64_t nodes_per_second = (result.nodes * 1000) / duration_ms.count();
        
        // Should achieve reasonable node rate (this is a basic test, full performance testing later)
        EXPECT_GT(nodes_per_second, 1000);  // Very basic threshold
    }
    
    EXPECT_GT(result.nodes, 100);  // Should search meaningful number of nodes
}

// UCI Integration Tests  
TEST_F(SearchEngineTest, UCIBridgeCompatibility) {
    // Test that SearchEngine works with existing UCIBridge interface
    board->setFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    
    SearchLimits limits;
    limits.max_depth = 3;
    limits.max_time_ms = 1000;
    
    // This tests the interface that will be used by UCIBridge.cpp
    EXPECT_NO_THROW({
        auto result = search_engine->search(limits);
        EXPECT_NE(result.best_move, NULL_MOVE);
    });
}

// Thread Safety Tests
TEST_F(SearchEngineTest, ConcurrentStopCalls) {
    board->setFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    
    SearchLimits limits;
    limits.max_depth = 8;
    limits.max_time_ms = 10000;
    
    std::atomic<bool> search_completed(false);
    SearchResult result;
    
    std::thread search_thread([&]() {
        result = search_engine->search(limits);
        search_completed.store(true);
    });
    
    // Multiple concurrent stop calls
    std::vector<std::thread> stop_threads;
    for (int i = 0; i < 3; ++i) {
        stop_threads.emplace_back([&]() {
            std::this_thread::sleep_for(std::chrono::milliseconds(30 + i * 10));
            search_engine->stop();
        });
    }
    
    search_thread.join();
    for (auto& t : stop_threads) {
        t.join();
    }
    
    EXPECT_TRUE(search_completed.load());
    EXPECT_NE(result.best_move, NULL_MOVE);
}

// Negative Tests - Testing Failure Cases
TEST_F(SearchEngineTest, InvalidDepthLimit) {
    board->setFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    
    SearchLimits limits;
    limits.max_depth = 0;  // Invalid depth
    
    // Should handle gracefully, likely by using a minimum depth
    EXPECT_NO_THROW({
        auto result = search_engine->search(limits);
        // Should still provide some result
        EXPECT_GE(result.depth, 0);
    });
}

TEST_F(SearchEngineTest, ZeroTimeLimit) {
    board->setFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    
    SearchLimits limits;
    limits.max_depth = 5;
    limits.max_time_ms = 0;  // No time
    
    // Should handle gracefully and return quickly
    auto start_time = std::chrono::high_resolution_clock::now();
    auto result = search_engine->search(limits);
    auto end_time = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    
    EXPECT_LT(duration.count(), 50);  // Should return very quickly
    EXPECT_GE(result.depth, 0);  // Should still provide some result
}