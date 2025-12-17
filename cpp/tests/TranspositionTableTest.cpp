#include <gtest/gtest.h>
#include <memory>
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>
#include "search/transposition_table.h"
#include "Types.h"

using namespace opera;

class TranspositionTableTest : public ::testing::Test {
protected:
    void SetUp() override {
        tt = std::make_unique<TranspositionTable>(64);  // 64MB table
    }

    void TearDown() override {
        tt.reset();
    }

    std::unique_ptr<TranspositionTable> tt;
    
    // Helper function to create test entries using packed API
    TTEntry createTestEntry(uint64_t zobrist_key, Move best_move, int score, 
                           int depth, TTEntryType type, int age = 0) {
        TTEntry entry;
        entry.set_data(zobrist_key, best_move, static_cast<int16_t>(score), 
                      static_cast<uint8_t>(depth), type, static_cast<uint8_t>(age));
        return entry;
    }
};

// Basic Construction and Interface Tests
TEST_F(TranspositionTableTest, DefaultConstruction) {
    EXPECT_NE(tt, nullptr);
    const auto& stats = tt->get_stats();
    EXPECT_EQ(stats.lookups.load(), 0);
    EXPECT_EQ(stats.hits.load(), 0);
    EXPECT_EQ(stats.get_hit_rate(), 0.0);
    EXPECT_GT(tt->size_mb(), 0);
}

TEST_F(TranspositionTableTest, CustomSizeConstruction) {
    auto tt_small = std::make_unique<TranspositionTable>(16);  // 16MB
    EXPECT_EQ(tt_small->size_mb(), 16);
    
    auto tt_large = std::make_unique<TranspositionTable>(128);  // 128MB
    EXPECT_EQ(tt_large->size_mb(), 128);
}

TEST_F(TranspositionTableTest, TTEntryStructure) {
    // Test TTEntry structure size and alignment
    EXPECT_LE(sizeof(TTEntry), 16);  // Should be compact for cache efficiency
    
    // Test TTCluster size (4 entries per cluster)
    EXPECT_EQ(TTCluster::CLUSTER_SIZE, 4);
    EXPECT_EQ(sizeof(TTCluster), 4 * sizeof(TTEntry));
}

// Basic Store/Probe Operations
TEST_F(TranspositionTableTest, BasicStoreAndProbe) {
    uint64_t zobrist_key = 0x123456789ABCDEF0ULL;
    Move best_move(E2, E4);
    int16_t score = 150;
    uint8_t depth = 8;
    TTEntryType type = TTEntryType::EXACT;

    // Store entry
    tt->store(zobrist_key, best_move, score, depth, type);
    
    // Probe entry
    TTEntry entry;
    bool found = tt->probe(zobrist_key, entry);
    
    EXPECT_TRUE(found);
    EXPECT_EQ(entry.get_move(), best_move);
    EXPECT_EQ(entry.get_score(), score);
    EXPECT_EQ(entry.get_depth(), depth);
    EXPECT_EQ(entry.get_type(), type);
}

TEST_F(TranspositionTableTest, ProbeNonExistentEntry) {
    uint64_t zobrist_key = 0x123456789ABCDEF0ULL;
    
    TTEntry entry;
    bool found = tt->probe(zobrist_key, entry);
    
    EXPECT_FALSE(found);
}

TEST_F(TranspositionTableTest, ContainsMethod) {
    uint64_t zobrist_key = 0x123456789ABCDEF0ULL;
    Move best_move(E2, E4);
    
    EXPECT_FALSE(tt->contains(zobrist_key));
    
    tt->store(zobrist_key, best_move, 150, 8, TTEntryType::EXACT);
    
    EXPECT_TRUE(tt->contains(zobrist_key));
}

// Clustering Tests
TEST_F(TranspositionTableTest, ClusteringBehavior) {
    // Test that multiple entries can be stored in the same cluster
    // by using keys that hash to the same cluster
    
    std::vector<uint64_t> keys;
    std::vector<Move> moves;
    
    // Create keys that should hash to similar clusters
    for (int i = 0; i < 4; ++i) {
        keys.push_back(0x1000000000000000ULL + i);
        moves.push_back(Move(static_cast<Square>(E2 + i), static_cast<Square>(E4 + i)));
    }
    
    // Store all entries
    for (size_t i = 0; i < keys.size(); ++i) {
        tt->store(keys[i], moves[i], static_cast<int16_t>(100 + i), 
                 static_cast<uint8_t>(5 + i), TTEntryType::EXACT);
    }
    
    // Verify all can be retrieved
    for (size_t i = 0; i < keys.size(); ++i) {
        TTEntry entry;
        bool found = tt->probe(keys[i], entry);
        EXPECT_TRUE(found) << "Failed to find entry " << i;
        if (found) {
            EXPECT_EQ(entry.get_move(), moves[i]);
            EXPECT_EQ(entry.get_score(), 100 + i);
            EXPECT_EQ(entry.get_depth(), 5 + i);
        }
    }
}

// Replacement Strategy Tests
TEST_F(TranspositionTableTest, ReplaceByDepthStrategy) {
    uint64_t base_key = 0x2000000000000000ULL;
    Move move1(E2, E4);
    Move move2(D2, D4);
    
    // Store shallow entry
    tt->store(base_key, move1, 100, 3, TTEntryType::EXACT);
    
    // Verify it's stored
    TTEntry entry;
    EXPECT_TRUE(tt->probe(base_key, entry));
    EXPECT_EQ(entry.get_depth(), 3);
    
    // Store deeper entry with same key (should replace)
    tt->store(base_key, move2, 200, 8, TTEntryType::EXACT);
    
    // Verify replacement occurred
    EXPECT_TRUE(tt->probe(base_key, entry));
    EXPECT_EQ(entry.get_move(), move2);
    EXPECT_EQ(entry.get_score(), 200);
    EXPECT_EQ(entry.get_depth(), 8);
}

TEST_F(TranspositionTableTest, ReplaceByAgeStrategy) {
    // Test age-based replacement when cluster is full
    uint64_t base_key = 0x3000000000000000ULL;
    
    // Fill a cluster with old entries
    for (int i = 0; i < 5; ++i) {  // Try to store 5 entries (more than cluster size)
        uint64_t key = base_key + i;
        Move move(static_cast<Square>(E2 + i), static_cast<Square>(E4 + i));
        tt->store(key, move, 100 + i, 5, TTEntryType::EXACT);
    }
    
    // Age the table
    tt->new_search();
    
    // Store a new entry - should replace an old one
    uint64_t new_key = base_key + 10;
    Move new_move(A1, A8);
    tt->store(new_key, new_move, 500, 10, TTEntryType::EXACT);
    
    // Verify new entry is stored
    TTEntry entry;
    EXPECT_TRUE(tt->probe(new_key, entry));
    EXPECT_EQ(entry.get_move(), new_move);
}

// Statistics Tests
TEST_F(TranspositionTableTest, StatisticsTracking) {
    uint64_t zobrist_key = 0x4000000000000000ULL;
    Move best_move(E2, E4);
    
    const auto& stats = tt->get_stats();
    
    // Initial state
    EXPECT_EQ(stats.lookups.load(), 0);
    EXPECT_EQ(stats.hits.load(), 0);
    EXPECT_EQ(stats.stores.load(), 0);
    
    // Store an entry
    tt->store(zobrist_key, best_move, 150, 8, TTEntryType::EXACT);
    EXPECT_EQ(stats.stores.load(), 1);
    
    // Successful probe (hit)
    TTEntry entry;
    tt->probe(zobrist_key, entry);
    EXPECT_EQ(stats.lookups.load(), 1);
    EXPECT_EQ(stats.hits.load(), 1);
    
    // Failed probe (miss)
    tt->probe(zobrist_key + 1, entry);
    EXPECT_EQ(stats.lookups.load(), 2);
    EXPECT_EQ(stats.hits.load(), 1);  // Still just one hit
    
    // Check hit rate
    EXPECT_DOUBLE_EQ(stats.get_hit_rate(), 0.5);  // 1 hit out of 2 lookups
}

TEST_F(TranspositionTableTest, StatisticsReset) {
    uint64_t zobrist_key = 0x5000000000000000ULL;
    Move best_move(E2, E4);
    
    // Generate some statistics
    tt->store(zobrist_key, best_move, 150, 8, TTEntryType::EXACT);
    TTEntry entry;
    tt->probe(zobrist_key, entry);
    
    const auto& stats = tt->get_stats();
    EXPECT_GT(stats.lookups.load(), 0);
    EXPECT_GT(stats.hits.load(), 0);
    
    // Reset and verify
    tt->reset_stats();
    EXPECT_EQ(stats.lookups.load(), 0);
    EXPECT_EQ(stats.hits.load(), 0);
    EXPECT_EQ(stats.stores.load(), 0);
}

// Clear Operation Tests
TEST_F(TranspositionTableTest, ClearOperation) {
    uint64_t zobrist_key = 0x6000000000000000ULL;
    Move best_move(E2, E4);
    
    // Store entry
    tt->store(zobrist_key, best_move, 150, 8, TTEntryType::EXACT);
    EXPECT_TRUE(tt->contains(zobrist_key));
    
    // Clear table
    tt->clear();
    EXPECT_FALSE(tt->contains(zobrist_key));
    
    // Verify statistics are also cleared
    const auto& stats = tt->get_stats();
    EXPECT_EQ(stats.lookups.load(), 0);
    EXPECT_EQ(stats.hits.load(), 0);
}

// Memory Management Tests
TEST_F(TranspositionTableTest, MemoryInfo) {
    auto [clusters, entries_per_cluster, bytes_per_cluster] = tt->get_memory_info();
    
    EXPECT_GT(clusters, 0);
    EXPECT_EQ(entries_per_cluster, 4);  // TTCluster::CLUSTER_SIZE
    EXPECT_EQ(bytes_per_cluster, sizeof(TTCluster));
    
    // Verify size calculation is consistent
    size_t expected_size = clusters * bytes_per_cluster;
    EXPECT_GE(tt->size_mb() * 1024 * 1024, expected_size / 2);  // Allow some overhead
}

// Entry Type Tests
TEST_F(TranspositionTableTest, DifferentEntryTypes) {
    uint64_t base_key = 0x7000000000000000ULL;
    Move move(E2, E4);
    
    // Test all entry types
    std::vector<TTEntryType> types = {
        TTEntryType::EXACT,
        TTEntryType::LOWER_BOUND,
        TTEntryType::UPPER_BOUND
    };
    
    for (size_t i = 0; i < types.size(); ++i) {
        uint64_t key = base_key + i;
        tt->store(key, move, 100, 5, types[i]);
        
        TTEntry entry;
        EXPECT_TRUE(tt->probe(key, entry));
        EXPECT_EQ(entry.get_type(), types[i]);
    }
}

// Performance Tests
TEST_F(TranspositionTableTest, PerformanceStoreProbe) {
    const int num_operations = 10000;
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    // Store many entries
    for (int i = 0; i < num_operations; ++i) {
        uint64_t key = static_cast<uint64_t>(i) * 0x123456789ULL;
        Move move(static_cast<Square>(i % 64), static_cast<Square>((i + 1) % 64));
        tt->store(key, move, i % 2000, i % 32, TTEntryType::EXACT);
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    // Should be fast - less than 100 microseconds per operation
    double avg_time_us = static_cast<double>(duration.count()) / num_operations;
    EXPECT_LT(avg_time_us, 100.0);
    
    // Verify hit rate is reasonable
    const auto& stats = tt->get_stats();
    EXPECT_GT(stats.stores.load(), 0);
}

// Edge Cases and Error Handling
TEST_F(TranspositionTableTest, ExtremeSizes) {
    // Test minimum size (should handle gracefully)
    auto tt_tiny = std::make_unique<TranspositionTable>(1);  // 1MB
    EXPECT_GE(tt_tiny->size_mb(), 1);
    
    // Basic functionality should still work
    tt_tiny->store(0x123ULL, Move(E2, E4), 100, 5, TTEntryType::EXACT);
    TTEntry entry;
    EXPECT_TRUE(tt_tiny->probe(0x123ULL, entry));
}

TEST_F(TranspositionTableTest, ZeroDepthAndScore) {
    uint64_t key = 0x8000000000000000ULL;
    Move move(E2, E4);
    
    // Test with zero depth and score
    tt->store(key, move, 0, 0, TTEntryType::EXACT);
    
    TTEntry entry;
    EXPECT_TRUE(tt->probe(key, entry));
    EXPECT_EQ(entry.get_score(), 0);
    EXPECT_EQ(entry.get_depth(), 0);
}

TEST_F(TranspositionTableTest, NegativeScores) {
    uint64_t key = 0x9000000000000000ULL;
    Move move(E2, E4);
    int16_t negative_score = -1500;
    
    tt->store(key, move, negative_score, 10, TTEntryType::EXACT);
    
    TTEntry entry;
    EXPECT_TRUE(tt->probe(key, entry));
    EXPECT_EQ(entry.get_score(), negative_score);
}

// Thread Safety Tests (Basic)
TEST_F(TranspositionTableTest, BasicThreadSafety) {
    const int num_threads = 4;
    const int ops_per_thread = 1000;
    
    std::vector<std::thread> threads;
    
    // Launch threads that perform concurrent operations
    for (int t = 0; t < num_threads; ++t) {
        threads.emplace_back([this, t, ops_per_thread]() {
            for (int i = 0; i < ops_per_thread; ++i) {
                uint64_t key = static_cast<uint64_t>(t * ops_per_thread + i) * 0x1000ULL;
                Move move(static_cast<Square>((t + i) % 64), static_cast<Square>((t + i + 1) % 64));
                
                tt->store(key, move, i, i % 32, TTEntryType::EXACT);
                
                TTEntry entry;
                tt->probe(key, entry);  // May or may not find it due to races, but shouldn't crash
            }
        });
    }
    
    // Wait for all threads
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Verify table is still functional
    const auto& stats = tt->get_stats();
    EXPECT_GT(stats.stores.load(), 0);
    EXPECT_GT(stats.lookups.load(), 0);
}

// Prefetch Test (verify it doesn't crash)
TEST_F(TranspositionTableTest, PrefetchOperation) {
    uint64_t key = 0xA000000000000000ULL;
    
    // Should not crash or throw
    EXPECT_NO_THROW(tt->prefetch(key));
    
    // Should still work normally after prefetch
    Move move(E2, E4);
    tt->store(key, move, 100, 5, TTEntryType::EXACT);
    
    TTEntry entry;
    EXPECT_TRUE(tt->probe(key, entry));
}