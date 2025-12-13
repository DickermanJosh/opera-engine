/**
 * @file evaluator_interface.h
 * @brief Abstract evaluator interface for Opera Chess Engine
 *
 * Provides strategy pattern abstraction enabling pluggable evaluation systems:
 * - Handcrafted evaluators (material + positional)
 * - Morphy-specialized evaluators (sacrificial style)
 * - Future neural network evaluators (NNUE, transformers)
 *
 * Design Philosophy:
 * - Pure virtual interface for maximum flexibility
 * - Optional incremental evaluation hooks for performance
 * - UCI option configuration support
 * - Evaluation from white's perspective with side-to-move adjustment
 */

#ifndef OPERA_EVALUATOR_INTERFACE_H
#define OPERA_EVALUATOR_INTERFACE_H

#include "Board.h"
#include "Types.h"
#include <map>
#include <string>

namespace opera {
namespace eval {

/**
 * @class Evaluator
 * @brief Abstract base class for position evaluation
 *
 * All evaluators implement this interface to provide consistent
 * evaluation semantics across different evaluation approaches.
 *
 * Contract Requirements:
 * 1. evaluate() must return score from white's perspective
 *    - Positive = white advantage
 *    - Negative = black advantage
 *    - Zero = equal position
 * 2. Evaluation must be symmetric (eval(position) = -eval(flipped))
 * 3. configure_options() must handle invalid options gracefully
 * 4. Incremental hooks (on_move_*) are optional optimizations
 *
 * Thread Safety:
 * - Evaluators are NOT required to be thread-safe
 * - Each search thread should have its own evaluator instance
 * - configure_options() should not be called during search
 *
 * Performance Requirements:
 * - evaluate() should complete in <1μs for competitive play
 * - Incremental updates can improve performance significantly
 * - Caching and lazy evaluation encouraged where appropriate
 */
class Evaluator {
public:
    /**
     * @brief Virtual destructor for proper cleanup
     */
    virtual ~Evaluator() = default;

    /**
     * @brief Evaluate the current position
     *
     * Returns a centipawn score from white's perspective. The evaluation
     * should consider:
     * - Material balance
     * - Positional factors (piece placement, mobility, king safety)
     * - Pawn structure
     * - Game phase (opening, middlegame, endgame)
     *
     * @param board The board position to evaluate
     * @param side_to_move The color whose turn it is to move
     * @return Centipawn score (positive = white advantage, negative = black)
     *
     * @note The side_to_move parameter allows the evaluator to apply
     *       tempo bonuses or side-to-move adjustments if desired.
     *
     * @warning This method should be const, but Board methods are not
     *          marked const. Future refactoring may add const correctness.
     */
    virtual int evaluate(const Board& board, Color side_to_move) = 0;

    /**
     * @brief Configure evaluation options from UCI
     *
     * Allows runtime configuration of evaluation parameters through
     * UCI options. Common options include:
     * - "MorphyBias": Morphy-style sacrificial bias (0.0-2.0)
     * - "PawnStructureWeight": Pawn structure emphasis (0-100)
     * - "KingSafetyWeight": King safety emphasis (0-100)
     * - "MobilityWeight": Piece mobility emphasis (0-100)
     *
     * @param options Map of option names to values (as strings)
     *
     * @note Implementations should:
     *       1. Validate option values (range checking)
     *       2. Handle unknown options gracefully (ignore or log warning)
     *       3. Apply changes immediately (no restart required)
     *       4. Use reasonable defaults if options not provided
     */
    virtual void configure_options(const std::map<std::string, std::string>& options) = 0;

    // ========================================================================
    // Optional Incremental Evaluation Hooks
    // ========================================================================

    /**
     * @brief Hook called when a move is made
     *
     * Optional optimization hook for incremental evaluation updates.
     * Allows evaluators to update cached state incrementally instead
     * of recomputing full evaluation from scratch.
     *
     * Example use cases:
     * - Update piece-square table scores
     * - Adjust material balance
     * - Update king safety metrics
     * - Modify pawn structure hash
     *
     * @param move The move that was just made
     *
     * @note Default implementation does nothing (no incremental update)
     * @note This is called AFTER the move has been made on the board
     * @note Implementation should be fast (<100ns) to be beneficial
     */
    virtual void on_move_made(Move move) {
        // Default: no incremental update
        // Concrete evaluators can override for optimization
    }

    /**
     * @brief Hook called when a move is undone
     *
     * Optional optimization hook for reverting incremental updates.
     * Should restore evaluator state to before the move was made.
     *
     * @param move The move that was just undone
     *
     * @note Default implementation does nothing (no incremental update)
     * @note This is called AFTER the move has been undone on the board
     * @note Must be symmetric with on_move_made() for correctness
     */
    virtual void on_move_undone(Move move) {
        // Default: no incremental update
    }

    /**
     * @brief Hook called when position is reset or changed completely
     *
     * Signals that incremental state should be discarded and
     * evaluation should be recalculated from scratch on next evaluate().
     *
     * Called in scenarios like:
     * - New game started
     * - Position loaded from FEN
     * - Search engine reset
     *
     * @note Default implementation does nothing (stateless evaluation)
     * @note Implementations with cached state should clear caches here
     */
    virtual void on_position_reset() {
        // Default: no cached state to reset
    }
};

} // namespace eval
} // namespace opera

#endif // OPERA_EVALUATOR_INTERFACE_H
