#pragma once

#include <atomic>
#include <memory>
#include <cstdint>
#include "Types.h"

namespace opera {

/**
 * Entry types for transposition table
 */
enum class TTEntryType : uint8_t {
    EXACT = 0,      // Exact score (PV node)
    LOWER_BOUND,    // Beta cutoff (fail-high)
    UPPER_BOUND     // Alpha cutoff (fail-low)
};

/**
 * Packed transposition table entry with clustering support
 * 128 bits total, designed for cache-friendly access
 */
struct TTEntry {
    uint64_t key_and_data;     // Upper 32 bits: partial key, lower 32 bits: packed data
    uint64_t move_and_eval;    // Upper 32 bits: move, lower 32 bits: score/depth/type/age
    
    // Constructor
    TTEntry() : key_and_data(0), move_and_eval(0) {}
    
    // Accessors for packed data
    uint32_t get_key() const { return static_cast<uint32_t>(key_and_data >> 32); }
    int16_t get_score() const { return static_cast<int16_t>(move_and_eval & 0xFFFF); }
    uint8_t get_depth() const { return static_cast<uint8_t>((move_and_eval >> 16) & 0xFF); }
    TTEntryType get_type() const { return static_cast<TTEntryType>((move_and_eval >> 24) & 0x3); }
    uint8_t get_age() const { return static_cast<uint8_t>((move_and_eval >> 26) & 0x3F); }
    Move get_move() const { 
        uint32_t move_data = static_cast<uint32_t>(move_and_eval >> 32);
        return Move(static_cast<Square>(move_data & 0x3F), 
                   static_cast<Square>((move_data >> 6) & 0x3F));
    }
    
    // Mutators for packed data
    void set_data(uint64_t zobrist_key, Move move, int16_t score, uint8_t depth, 
                  TTEntryType type, uint8_t age) {
        // Store upper 32 bits of zobrist key
        key_and_data = (zobrist_key & 0xFFFFFFFF00000000ULL) | 
                       static_cast<uint64_t>(depth);
        
        // Pack move into upper 32 bits
        uint32_t move_data = (static_cast<uint32_t>(move.to()) << 6) | 
                            static_cast<uint32_t>(move.from());
        
        // Pack score, depth, type, age into lower 32 bits
        move_and_eval = (static_cast<uint64_t>(move_data) << 32) |
                       (static_cast<uint64_t>(age & 0x3F) << 26) |
                       (static_cast<uint64_t>(type) << 24) |
                       (static_cast<uint64_t>(depth) << 16) |
                       static_cast<uint64_t>(static_cast<uint16_t>(score));
    }
    
    bool matches_key(uint64_t zobrist_key) const {
        return get_key() == static_cast<uint32_t>(zobrist_key >> 32);
    }
};

/**
 * Cluster of transposition table entries for better cache utilization
 */
struct TTCluster {
    static constexpr int CLUSTER_SIZE = 4;
    TTEntry entries[CLUSTER_SIZE];
    
    TTCluster() = default;
};

/**
 * Statistics for transposition table performance monitoring
 */
struct TTStats {
    mutable std::atomic<uint64_t> lookups{0};
    mutable std::atomic<uint64_t> hits{0};
    mutable std::atomic<uint64_t> stores{0};
    mutable std::atomic<uint64_t> overwrites{0};
    mutable std::atomic<uint64_t> collisions{0};
    
    void reset() {
        lookups.store(0);
        hits.store(0);
        stores.store(0);
        overwrites.store(0);
        collisions.store(0);
    }
    
    double get_hit_rate() const {
        uint64_t total_lookups = lookups.load();
        return total_lookups > 0 ? static_cast<double>(hits.load()) / total_lookups : 0.0;
    }
};

/**
 * High-performance transposition table with clustering and replacement strategy
 */
class TranspositionTable {
private:
    std::unique_ptr<TTCluster[]> table;
    size_t cluster_count;
    size_t size_bytes;
    uint8_t current_age;
    TTStats stats;
    
    // Hash function for cluster index
    size_t get_cluster_index(uint64_t zobrist_key) const {
        return (zobrist_key ^ (zobrist_key >> 32)) % cluster_count;
    }
    
    // Replace-by-depth strategy with age consideration
    int find_replace_index(TTCluster& cluster, uint64_t zobrist_key, uint8_t depth) const;

public:
    /**
     * Constructor with size in MB
     * @param size_mb Size of table in megabytes
     */
    explicit TranspositionTable(size_t size_mb = 64);
    
    /**
     * Destructor
     */
    ~TranspositionTable() = default;
    
    // Non-copyable
    TranspositionTable(const TranspositionTable&) = delete;
    TranspositionTable& operator=(const TranspositionTable&) = delete;
    
    /**
     * Store entry in transposition table
     * @param zobrist_key Position's zobrist hash
     * @param move Best move for position
     * @param score Position evaluation
     * @param depth Search depth
     * @param type Entry type (exact, lower bound, upper bound)
     */
    void store(uint64_t zobrist_key, Move move, int16_t score, uint8_t depth, TTEntryType type);
    
    /**
     * Probe transposition table for position
     * @param zobrist_key Position's zobrist hash
     * @param entry Output parameter for found entry
     * @return true if entry found and valid
     */
    bool probe(uint64_t zobrist_key, TTEntry& entry) const;
    
    /**
     * Check if position exists in table (lighter than full probe)
     * @param zobrist_key Position's zobrist hash
     * @return true if position found
     */
    bool contains(uint64_t zobrist_key) const;
    
    /**
     * Clear all entries in the table
     */
    void clear();
    
    /**
     * Age the table (increment generation)
     */
    void new_search() { current_age = (current_age + 1) % 64; }
    
    /**
     * Get table size in MB
     * @return Size in megabytes
     */
    size_t size_mb() const { return size_bytes / (1024 * 1024); }
    
    /**
     * Get table statistics
     * @return Reference to statistics object
     */
    const TTStats& get_stats() const { return stats; }
    
    /**
     * Reset statistics counters
     */
    void reset_stats() { stats.reset(); }
    
    /**
     * Get memory usage details
     * @return Tuple of (total_clusters, entries_per_cluster, bytes_per_cluster)
     */
    std::tuple<size_t, size_t, size_t> get_memory_info() const {
        return std::make_tuple(cluster_count, TTCluster::CLUSTER_SIZE, sizeof(TTCluster));
    }
    
    /**
     * Prefetch cluster for better performance
     * @param zobrist_key Position's zobrist hash
     */
    void prefetch(uint64_t zobrist_key) const;
};

} // namespace opera