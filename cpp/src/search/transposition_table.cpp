#include "search/transposition_table.h"
#include <algorithm>
#include <cstring>
#include <climits>

// Platform-specific prefetch support
#ifdef __GNUC__
    #define PREFETCH(addr) __builtin_prefetch(addr, 0, 1)
#elif defined(_MSC_VER)
    #include <intrin.h>
    #define PREFETCH(addr) _mm_prefetch((const char*)(addr), _MM_HINT_T1)
#else
    #define PREFETCH(addr) ((void)0)
#endif

namespace opera {

TranspositionTable::TranspositionTable(size_t size_mb) 
    : current_age(0) {
    
    // Calculate cluster count based on desired size
    size_bytes = size_mb * 1024 * 1024;
    cluster_count = size_bytes / sizeof(TTCluster);
    
    // Ensure minimum size
    if (cluster_count < 1024) {
        cluster_count = 1024;
        size_bytes = cluster_count * sizeof(TTCluster);
    }
    
    // Allocate table
    table = std::make_unique<TTCluster[]>(cluster_count);
    
    // Initialize all entries to zero
    clear();
}

void TranspositionTable::store(uint64_t zobrist_key, Move move, int16_t score, 
                              uint8_t depth, TTEntryType type) {
    stats.stores.fetch_add(1, std::memory_order_relaxed);
    
    size_t cluster_idx = get_cluster_index(zobrist_key);
    TTCluster& cluster = table[cluster_idx];
    
    // Check if key already exists (update case)
    for (int i = 0; i < TTCluster::CLUSTER_SIZE; ++i) {
        if (cluster.entries[i].matches_key(zobrist_key)) {
            cluster.entries[i].set_data(zobrist_key, move, score, depth, type, current_age);
            stats.overwrites.fetch_add(1, std::memory_order_relaxed);
            return;
        }
    }
    
    // Find slot to replace using replace-by-depth strategy
    int replace_idx = find_replace_index(cluster, zobrist_key, depth);
    
    // Check if we're replacing a valid entry
    if (cluster.entries[replace_idx].get_key() != 0) {
        stats.collisions.fetch_add(1, std::memory_order_relaxed);
    }
    
    cluster.entries[replace_idx].set_data(zobrist_key, move, score, depth, type, current_age);
}

bool TranspositionTable::probe(uint64_t zobrist_key, TTEntry& entry) const {
    stats.lookups.fetch_add(1, std::memory_order_relaxed);
    
    size_t cluster_idx = get_cluster_index(zobrist_key);
    const TTCluster& cluster = table[cluster_idx];
    
    for (int i = 0; i < TTCluster::CLUSTER_SIZE; ++i) {
        if (cluster.entries[i].matches_key(zobrist_key)) {
            entry = cluster.entries[i];
            stats.hits.fetch_add(1, std::memory_order_relaxed);
            return true;
        }
    }
    
    return false;
}

bool TranspositionTable::contains(uint64_t zobrist_key) const {
    size_t cluster_idx = get_cluster_index(zobrist_key);
    const TTCluster& cluster = table[cluster_idx];
    
    for (int i = 0; i < TTCluster::CLUSTER_SIZE; ++i) {
        if (cluster.entries[i].matches_key(zobrist_key)) {
            return true;
        }
    }
    
    return false;
}

void TranspositionTable::clear() {
    // Zero out entire table
    std::memset(table.get(), 0, cluster_count * sizeof(TTCluster));
    stats.reset();
}

int TranspositionTable::find_replace_index(TTCluster& cluster, uint64_t /* zobrist_key */, 
                                          uint8_t depth) const {
    int best_idx = 0;
    int lowest_priority = INT_MAX;
    
    for (int i = 0; i < TTCluster::CLUSTER_SIZE; ++i) {
        // Empty slot - use immediately
        if (cluster.entries[i].get_key() == 0) {
            return i;
        }
        
        // Calculate replacement priority
        // Lower priority = more likely to be replaced
        int priority = 0;
        
        // Prefer replacing older entries
        uint8_t age_diff = (current_age - cluster.entries[i].get_age()) % 64;
        priority += age_diff * 4;  // Age weight
        
        // Prefer replacing shallower entries
        priority += std::max(0, static_cast<int>(depth) - static_cast<int>(cluster.entries[i].get_depth()));
        
        if (priority < lowest_priority) {
            lowest_priority = priority;
            best_idx = i;
        }
    }
    
    return best_idx;
}

void TranspositionTable::prefetch(uint64_t zobrist_key) const {
    size_t cluster_idx = get_cluster_index(zobrist_key);
    const TTCluster* cluster_ptr = &table[cluster_idx];
    
    PREFETCH(cluster_ptr);
}

} // namespace opera