#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include <chrono>
#include "Board.h"
#include "MoveGen.h"
#include "Types.h"
#include "search/alphabeta.h"
#include "search/transposition_table.h"
#include "search/move_ordering.h"
#include "search/see.h"

namespace opera {

// Null move constant for search engine
const Move NULL_MOVE = Move();

/**
 * Search limits and constraints for the search engine
 */
struct SearchLimits {
    int max_depth = 64;                    // Maximum search depth
    uint64_t max_nodes = UINT64_MAX;       // Maximum nodes to search
    uint64_t max_time_ms = UINT64_MAX;     // Maximum time in milliseconds
    bool infinite = false;                 // Infinite search mode
    
    SearchLimits() = default;
    
    /**
     * Check if search should stop based on current progress
     */
    bool should_stop(int current_depth, uint64_t nodes, uint64_t elapsed_ms) const;
};

/**
 * Search result containing best move and search statistics
 */
struct SearchResult {
    Move best_move = NULL_MOVE;            // Best move found
    Move ponder_move = NULL_MOVE;          // Move to ponder on
    int score = 0;                         // Position evaluation in centipawns
    int depth = 0;                         // Depth searched
    uint64_t nodes = 0;                    // Total nodes searched
    uint64_t time_ms = 0;                  // Time taken in milliseconds
    std::vector<Move> principal_variation; // Principal variation
    
    SearchResult() = default;
};

/**
 * Search information for progress reporting during search
 */
struct SearchInfo {
    int depth = 0;                         // Current depth
    int score = 0;                         // Current score
    uint64_t time_ms = 0;                  // Elapsed time
    uint64_t nodes = 0;                    // Nodes searched so far
    uint64_t nps = 0;                      // Nodes per second
    std::string pv = "";                   // Principal variation string
    
    SearchInfo() = default;
};

/**
 * Main search engine class coordinating iterative deepening search
 * with UCI integration and async cancellation support
 */
class SearchEngine {
private:
    Board& board;                          // Reference to game board
    std::atomic<bool>& stop_flag;          // Atomic stop flag for async cancellation
    
    // Search components
    std::unique_ptr<TranspositionTable> tt;          // Transposition table
    std::unique_ptr<MoveOrdering> move_ordering;     // Move ordering system
    std::unique_ptr<StaticExchangeEvaluator> see;    // Static exchange evaluator
    std::unique_ptr<AlphaBetaSearch> alphabeta;      // Alpha-beta search engine
    
    // Search state
    bool searching = false;                // Currently searching flag
    SearchLimits current_limits;           // Current search limits
    SearchInfo current_info;               // Current search information
    std::chrono::high_resolution_clock::time_point search_start_time;
    
    // Search statistics (now delegated to AlphaBetaSearch)
    uint64_t nodes_searched = 0;           // Total nodes searched this session
    std::vector<Move> pv_line;             // Current principal variation
    
public:
    /**
     * Construct SearchEngine with board reference and stop flag
     * 
     * @param board Reference to the chess board
     * @param stop_flag Atomic boolean for async search cancellation
     */
    explicit SearchEngine(Board& board, std::atomic<bool>& stop_flag);
    
    /**
     * Destructor ensures search is stopped cleanly
     */
    ~SearchEngine();
    
    // Non-copyable
    SearchEngine(const SearchEngine&) = delete;
    SearchEngine& operator=(const SearchEngine&) = delete;
    
    /**
     * Start search with given limits using iterative deepening
     * 
     * @param limits Search constraints (depth, time, nodes)
     * @return SearchResult with best move and statistics
     */
    SearchResult search(const SearchLimits& limits);
    
    /**
     * Stop current search immediately
     * Sets stop flag and waits for search to terminate cleanly
     */
    void stop();
    
    /**
     * Check if currently searching
     * 
     * @return true if search is in progress
     */
    bool is_searching() const;
    
    /**
     * Get total nodes searched in current session
     * 
     * @return Total nodes searched
     */
    uint64_t get_nodes_searched() const;
    
    /**
     * Get current search information for progress reporting
     * 
     * @return Current SearchInfo with depth, score, nodes, etc.
     */
    const SearchInfo& get_search_info() const;
    
    /**
     * Reset search statistics (for new game)
     */
    void reset_statistics();
    
    /**
     * Configure AlphaBetaSearch optimization parameters (UCI options)
     */
    void set_null_move_reduction(int reduction);
    void set_lmr_full_depth_moves(int moves);
    void set_lmr_reduction_limit(int limit);
    void set_futility_margin(int margin);
    void set_razoring_margin(int margin);
    void set_min_depth_for_nmp(int depth);
    void set_min_depth_for_lmr(int depth);
    void set_min_depth_for_futility(int depth);
    void set_min_depth_for_razoring(int depth);
    
    /**
     * Get current AlphaBetaSearch optimization parameters
     */
    int get_null_move_reduction() const;
    int get_lmr_full_depth_moves() const;
    int get_lmr_reduction_limit() const;
    int get_futility_margin() const;
    int get_razoring_margin() const;
    int get_min_depth_for_nmp() const;
    int get_min_depth_for_lmr() const;
    int get_min_depth_for_futility() const;
    int get_min_depth_for_razoring() const;

private:
    /**
     * Main iterative deepening search loop
     * 
     * @return SearchResult with best move and statistics
     */
    SearchResult iterative_deepening();
    
    /**
     * Single depth search with aspiration windows using AlphaBetaSearch
     * 
     * @param depth Target search depth
     * @param prev_score Previous iteration score for aspiration window
     * @return Score from search
     */
    int aspiration_search(int depth, int prev_score);
    
    /**
     * Update search info for progress reporting
     * 
     * @param depth Current depth
     * @param score Current score
     * @param nodes Current node count
     */
    void update_search_info(int depth, int score, uint64_t nodes);
    
    /**
     * Check if search should stop based on limits and stop flag
     * 
     * @return true if search should terminate
     */
    bool should_stop_search();
    
    /**
     * Get elapsed time since search start in milliseconds
     * 
     * @return Elapsed time in milliseconds
     */
    uint64_t get_elapsed_time_ms() const;
    
    /**
     * Convert principal variation to string for reporting
     * 
     * @return PV as space-separated move string
     */
    std::string pv_to_string() const;
};

} // namespace opera