#pragma once

#include <atomic>
#include <vector>
#include <chrono>
#include "Board.h"
#include "MoveGen.h"
#include "Types.h"
#include "search/transposition_table.h"
#include "search/move_ordering.h"
#include "search/see.h"

namespace opera {

// Search constants
constexpr int INFINITY_SCORE = 32000;
constexpr int CHECKMATE_SCORE = 30000;
constexpr int MAX_PLY = 64;

// Extension values in plies
constexpr int CHECK_EXTENSION = 1;
constexpr int SINGULAR_EXTENSION = 1;
constexpr int PASSED_PAWN_EXTENSION = 1;

// Default search optimization constants  
constexpr int DEFAULT_NULL_MOVE_REDUCTION = 3;          // R=3 for null move pruning
constexpr int DEFAULT_LMR_FULL_DEPTH_MOVES = 4;        // First N moves get full depth
constexpr int DEFAULT_LMR_REDUCTION_LIMIT = 3;         // Maximum LMR reduction
constexpr int DEFAULT_FUTILITY_MARGIN = 200;           // Futility pruning margin
constexpr int DEFAULT_RAZORING_MARGIN = 300;           // Razoring margin
constexpr int DEFAULT_MIN_DEPTH_FOR_NMP = 3;           // Minimum depth for null move pruning
constexpr int DEFAULT_MIN_DEPTH_FOR_LMR = 2;           // Minimum depth for late move reductions
constexpr int DEFAULT_MIN_DEPTH_FOR_FUTILITY = 1;     // Minimum depth for futility pruning
constexpr int DEFAULT_MIN_DEPTH_FOR_RAZORING = 2;     // Minimum depth for razoring

/**
 * Search statistics for performance analysis
 */
struct SearchStats {
    uint64_t nodes = 0;                    // Total nodes searched
    uint64_t beta_cutoffs = 0;             // Beta cutoffs (fail-high)
    uint64_t first_move_cutoffs = 0;       // Beta cutoffs on first move
    uint64_t tt_hits = 0;                  // Transposition table hits
    uint64_t tt_cutoffs = 0;               // TT cutoffs
    uint64_t extensions = 0;               // Total extensions applied
    uint64_t reductions = 0;               // Total reductions applied
    
    // Search optimization statistics
    uint64_t null_move_cutoffs = 0;        // Null move pruning cutoffs
    uint64_t lmr_reductions = 0;           // Late move reductions applied
    uint64_t futility_prunes = 0;          // Futility pruning cutoffs
    uint64_t razoring_prunes = 0;          // Razoring pruning cutoffs
    
    // Reset all statistics
    void reset() {
        nodes = beta_cutoffs = first_move_cutoffs = 0;
        tt_hits = tt_cutoffs = extensions = reductions = 0;
        null_move_cutoffs = lmr_reductions = futility_prunes = razoring_prunes = 0;
    }
    
    // Get move ordering effectiveness (first move cutoff rate)
    double get_move_ordering_effectiveness() const {
        return beta_cutoffs > 0 ? (double)first_move_cutoffs / beta_cutoffs : 0.0;
    }
};

/**
 * Principal Variation Search (PVS) implementation of alpha-beta algorithm
 * with transposition table, move ordering, and search extensions
 */
class AlphaBetaSearch {
private:
    Board& board;                           // Reference to game board
    std::atomic<bool>& stop_flag;           // Atomic stop flag for cancellation
    TranspositionTable& tt;                 // Transposition table
    MoveOrdering& move_ordering;            // Move ordering system
    StaticExchangeEvaluator& see;           // Static exchange evaluation
    
    // Search state
    SearchStats stats;                      // Search statistics
    std::vector<Move> pv_line;              // Principal variation
    std::vector<std::vector<Move>> pv_table; // PV table for all depths
    
    // Killer moves (non-capture moves that cause beta cutoffs)
    Move killer_moves[MAX_PLY][2];          // Two killer moves per ply
    
    // History heuristic (move success rates)
    int history_table[64][64];              // From/to square history scores
    
    // Search control
    std::chrono::high_resolution_clock::time_point search_start_time;
    uint64_t node_check_counter = 0;        // Counter for periodic stop checks
    
    // Configurable search optimization parameters
    int null_move_reduction = DEFAULT_NULL_MOVE_REDUCTION;
    int lmr_full_depth_moves = DEFAULT_LMR_FULL_DEPTH_MOVES;
    int lmr_reduction_limit = DEFAULT_LMR_REDUCTION_LIMIT;
    int futility_margin = DEFAULT_FUTILITY_MARGIN;
    int razoring_margin = DEFAULT_RAZORING_MARGIN;
    int min_depth_for_nmp = DEFAULT_MIN_DEPTH_FOR_NMP;
    int min_depth_for_lmr = DEFAULT_MIN_DEPTH_FOR_LMR;
    int min_depth_for_futility = DEFAULT_MIN_DEPTH_FOR_FUTILITY;
    int min_depth_for_razoring = DEFAULT_MIN_DEPTH_FOR_RAZORING;
    
public:
    /**
     * Construct AlphaBetaSearch with required components
     * 
     * @param board Reference to the chess board
     * @param stop_flag Atomic boolean for search cancellation
     * @param tt Transposition table for position caching
     * @param move_ordering Move ordering system
     * @param see Static exchange evaluator
     */
    AlphaBetaSearch(Board& board, std::atomic<bool>& stop_flag,
                   TranspositionTable& tt, MoveOrdering& move_ordering,
                   StaticExchangeEvaluator& see);
    
    /**
     * Start search from root position
     * 
     * @param depth Maximum depth to search
     * @param alpha Initial alpha bound
     * @param beta Initial beta bound
     * @return Best score found
     */
    int search(int depth, int alpha = -INFINITY_SCORE, int beta = INFINITY_SCORE);
    
    /**
     * Principal Variation Search implementation
     * 
     * @param depth Remaining depth to search
     * @param ply Current ply from root (0 = root)
     * @param alpha Alpha bound (lower bound)
     * @param beta Beta bound (upper bound)
     * @param is_pv_node True if this is a principal variation node
     * @return Position evaluation score
     */
    int pvs(int depth, int ply, int alpha, int beta, bool is_pv_node);
    
    /**
     * Quiescence search to resolve tactical sequences
     * 
     * @param ply Current ply from root
     * @param alpha Alpha bound
     * @param beta Beta bound
     * @return Position evaluation after tactical resolution
     */
    int quiescence(int ply, int alpha, int beta);
    
    /**
     * Get the principal variation found by search
     * 
     * @return Vector of moves representing the PV
     */
    const std::vector<Move>& get_principal_variation() const;
    
    /**
     * Get current search statistics
     * 
     * @return SearchStats structure with performance data
     */
    const SearchStats& get_stats() const;
    
    /**
     * Reset search statistics and state
     */
    void reset();
    
    /**
     * Clear killer moves and history tables
     */
    void clear_history();
    
    /**
     * Configure search optimization parameters
     */
    void set_null_move_reduction(int reduction) { null_move_reduction = reduction; }
    void set_lmr_full_depth_moves(int moves) { lmr_full_depth_moves = moves; }
    void set_lmr_reduction_limit(int limit) { lmr_reduction_limit = limit; }
    void set_futility_margin(int margin) { futility_margin = margin; }
    void set_razoring_margin(int margin) { razoring_margin = margin; }
    void set_min_depth_for_nmp(int depth) { min_depth_for_nmp = depth; }
    void set_min_depth_for_lmr(int depth) { min_depth_for_lmr = depth; }
    void set_min_depth_for_futility(int depth) { min_depth_for_futility = depth; }
    void set_min_depth_for_razoring(int depth) { min_depth_for_razoring = depth; }
    
    /**
     * Get current search optimization parameters
     */
    int get_null_move_reduction() const { return null_move_reduction; }
    int get_lmr_full_depth_moves() const { return lmr_full_depth_moves; }
    int get_lmr_reduction_limit() const { return lmr_reduction_limit; }
    int get_futility_margin() const { return futility_margin; }
    int get_razoring_margin() const { return razoring_margin; }
    int get_min_depth_for_nmp() const { return min_depth_for_nmp; }
    int get_min_depth_for_lmr() const { return min_depth_for_lmr; }
    int get_min_depth_for_futility() const { return min_depth_for_futility; }
    int get_min_depth_for_razoring() const { return min_depth_for_razoring; }
    
    // Testing methods (public for validation)
    int get_lmr_reduction(int depth, int move_number, bool is_pv_node, const MoveGen& move) const;
    bool can_futility_prune(int depth, int alpha, int static_eval) const;
    bool can_razor(int depth, int alpha, int static_eval) const;

private:
    /**
     * Evaluate the current position
     * 
     * @return Static evaluation in centipawns
     */
    int evaluate();
    
    /**
     * Check if the current position is in check
     * 
     * @return True if side to move is in check
     */
    bool in_check() const;
    
    /**
     * Apply search extensions based on position characteristics
     * 
     * @param move The move being searched
     * @param in_check True if position is in check
     * @param gives_check True if move gives check
     * @return Extension amount in plies
     */
    int get_extensions(const MoveGen& move, bool in_check, bool gives_check);
    
    /**
     * Update killer moves when a non-capture causes beta cutoff
     * 
     * @param move The move that caused the cutoff
     * @param ply Current ply
     */
    void update_killers(const Move& move, int ply);
    
    /**
     * Update history heuristic scores
     * 
     * @param move The move that caused a cutoff
     * @param depth Depth bonus for the move
     */
    void update_history(const Move& move, int depth);
    
    /**
     * Check if search should stop (time/node limits or stop flag)
     * 
     * @return True if search should terminate
     */
    bool should_stop();
    
    /**
     * Convert internal Move to MoveGen for consistency
     * 
     * @param move The Move to convert
     * @return Equivalent MoveGen
     */
    MoveGen move_to_movegen(const Move& move) const;
    
    /**
     * Convert MoveGen to internal Move
     * 
     * @param mg The MoveGen to convert
     * @return Equivalent Move
     */
    Move movegen_to_move(const MoveGen& mg) const;
    
    /**
     * Extract principal variation from PV table
     * 
     * @param ply Starting ply
     */
    void extract_pv(int ply);
    
    /**
     * Check if null move pruning is allowed in current position
     * 
     * @param in_check True if current position is in check
     * @return True if null move is allowed
     */
    bool can_do_null_move(bool in_check) const;
    
    /**
     * Check if search should stop (time/external signal)
     * 
     * @return True if search should terminate
     */
    bool should_stop() const;
    
    /**
     * Make a null move (pass the turn to opponent)
     */
    void make_null_move();
    
    /**
     * Unmake the null move
     */
    void unmake_null_move();
    
};

} // namespace opera