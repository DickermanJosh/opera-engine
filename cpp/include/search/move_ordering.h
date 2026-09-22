#pragma once

#include <array>
#include <atomic>
#include <algorithm>
#include "search/see.h"
#include <thread>
#include <mutex>
#include "Board.h"
#include "MoveGen.h"
#include "Types.h"
#include "search/transposition_table.h"

namespace opera {

/**
 * Comprehensive move ordering system with multi-stage scoring for chess search optimization
 * 
 * Implements a hierarchical scoring system:
 * Hash move, non-losing captures/promotions, killers, quiet history, then
 * losing captures. Fixed-capacity storage avoids per-node allocations.
 */
class MoveOrdering {
public:
    // Scoring constants - hierarchical ordering
    static constexpr int TT_MOVE_SCORE = 1000000;
    static constexpr int GOOD_CAPTURE_BASE = 100000;
    static constexpr int KILLER_MOVE_SCORE = 90000;
    static constexpr int CHECK_MOVE_SCORE = 80000;
    static constexpr int HISTORY_MAX_SCORE = 1000;
    static constexpr int BAD_CAPTURE_BASE = -100000;
    
    // Killer move configuration
    static constexpr int MAX_SEARCH_DEPTH = 64;
    static constexpr int KILLERS_PER_DEPTH = 2;
    
    // History table configuration
    static constexpr int HISTORY_MAX_VALUE = 10000;
    static constexpr int HISTORY_AGING_DIVISOR = 8;
    
    // Piece values for MVV-LVA (in centipawns)
    static constexpr int PIECE_VALUES[7] = {100, 320, 330, 500, 900, 20000, 0};

private:
    Board& board;
    TranspositionTable& tt;
    
    // Killer moves table [depth][killer_index]
    std::array<std::array<MoveGen, KILLERS_PER_DEPTH>, MAX_SEARCH_DEPTH> killer_moves;
    
    // History heuristic table [color][from_square][to_square]
    std::array<std::array<std::array<std::atomic<int>, 64>, 64>, 2> history_table;
    
    // Move scores for current move list (optimization to avoid recalculation)
    struct ScoredMove { MoveGen move; int score; };
    std::array<ScoredMove, 256> move_scores;
    size_t scored_count = 0;
    StaticExchangeEvaluator see;
    int score_without_tt(const MoveGen& move, int depth);
    static bool matches(const MoveGen& move, const Move& candidate);
    
    // Thread safety
    mutable std::mutex killer_mutex;
    
public:
    /**
     * Constructor
     * @param board Reference to the chess board
     * @param tt Reference to transposition table for TT move identification
     */
    explicit MoveOrdering(Board& board, TranspositionTable& tt);
    
    /**
     * Destructor
     */
    ~MoveOrdering() = default;
    
    // Non-copyable
    MoveOrdering(const MoveOrdering&) = delete;
    MoveOrdering& operator=(const MoveOrdering&) = delete;
    
    /**
     * Score a single move using multi-stage evaluation
     * @param move The move to score
     * @param depth Current search depth for killer move lookup
     * @return Move score (higher = better priority)
     */
    int score_move(const MoveGen& move, int depth);
    
    /**
     * Score all moves in a move list
     * @param moves Move list to score
     * @param depth Current search depth
     */
    template<size_t MAX_MOVES>
    void score_moves(MoveGenList<MAX_MOVES>& moves, int depth);
    
    /**
     * Sort moves by their scores in descending order (best moves first)
     * @param moves Move list to sort (must be pre-scored)
     */
    template<size_t MAX_MOVES>
    void sort_moves(MoveGenList<MAX_MOVES>& moves);
    
    /**
     * Get the score for a previously scored move
     * @param move The move to get score for
     * @return Move score, or 0 if not found
     */
    int get_move_score(const MoveGen& move) const;
    
    /**
     * Store a killer move at a specific depth
     * @param move The killer move
     * @param depth Search depth where move caused beta cutoff
     */
    void store_killer_move(const MoveGen& move, int depth);
    
    /**
     * Get a killer move for a specific depth
     * @param depth Search depth
     * @param index Killer index (0 or 1)
     * @return Killer move, or NULL_MOVE_GEN if none stored
     */
    MoveGen get_killer_move(int depth, int index) const;
    
    /**
     * Update history heuristic for a move that caused a beta cutoff
     * @param move The move that was good
     * @param side Color of the side that played the move
     * @param depth Depth bonus (deeper = more valuable)
     */
    void update_history(const MoveGen& move, Color side, int depth);
    
    /**
     * Get history score for a move
     * @param move The move to get history for
     * @param side Color of the side playing the move
     * @return History score (0-HISTORY_MAX_SCORE)
     */
    int get_history_score(const MoveGen& move, Color side) const;
    
    /**
     * Age history table to prevent saturation (call periodically)
     */
    void age_history();
    
    /**
     * Clear all killer moves (for new game)
     */
    void clear_killers();
    
    /**
     * Clear history table (for new game)  
     */
    void clear_history();
    
    /**
     * Reset all move ordering data
     */
    void reset();

private:
    /**
     * Check if a move is stored in the transposition table
     * @param move Move to check
     * @return true if move is the TT best move
     */
    bool is_tt_move(const MoveGen& move) const;
    
    /**
     * Check if a move is a killer move at the given depth
     * @param move Move to check
     * @param depth Search depth
     * @return true if move is a killer
     */
    bool is_killer_move(const MoveGen& move, int depth) const;
    
    /**
     * Calculate MVV-LVA score for a capture
     * @param move The capture move
     * @return MVV-LVA score (higher for better captures)
     */
    int calculate_mvv_lva_score(const MoveGen& move) const;
    
    /**
     * Check if a capture is "good" or "bad"
     * @param move The capture move
     * @return true if capture is beneficial
     */
    bool is_good_capture(const MoveGen& move) const;
    
    /**
     * Get piece value for MVV-LVA calculation
     * @param piece Piece to evaluate
     * @return Piece value in centipawns
     */
    int get_piece_value(Piece piece) const;
    
    /**
     * Get piece type value for MVV-LVA calculation
     * @param piece_type PieceType to evaluate
     * @return Piece value in centipawns
     */
    int get_piece_type_value(PieceType piece_type) const;
    
    /**
     * Convert MoveGen to hash key for move_scores map
     * @param move Move to hash
     * @return Hash key
     */
    uint32_t move_to_key(const MoveGen& move) const;
};

// Template implementations

template<size_t MAX_MOVES>
void MoveOrdering::score_moves(MoveGenList<MAX_MOVES>& moves, int depth) {
    static_assert(MAX_MOVES <= 256, "Move-ordering capacity exceeded");
    TTEntry entry;
    const bool hit = tt.probe(board.getZobristKey(), entry);
    const Move tt_move = hit ? entry.get_move() : Move();
    scored_count = moves.size();
    for (size_t i = 0; i < moves.size(); ++i) {
        move_scores[i] = {moves[i], hit && matches(moves[i], tt_move) ?
                         TT_MOVE_SCORE : score_without_tt(moves[i], depth)};
    }
}

template<size_t MAX_MOVES>
void MoveOrdering::sort_moves(MoveGenList<MAX_MOVES>& moves) {
    std::sort(move_scores.begin(), move_scores.begin() + scored_count,
              [](const ScoredMove& a, const ScoredMove& b) {
                  return a.score > b.score || (a.score == b.score && a.move.rawData() < b.move.rawData());
              });
    for (size_t i = 0; i < scored_count; ++i) moves[i] = move_scores[i].move;
}

} // namespace opera
