/**
 * @file handcrafted_eval.h
 * @brief Traditional handcrafted chess evaluation with piece-square tables
 *
 * Implements classical chess evaluation covering:
 * - Material balance with standard piece values
 * - Piece-square tables for positional evaluation
 * - Tapered evaluation (opening/middlegame/endgame interpolation)
 * - Performance-optimized for <1μs evaluation time
 *
 * Design Philosophy:
 * - Material values: P=100, N=320, B=330, R=500, Q=900
 * - Evaluation from white's perspective (positive = white advantage)
 * - Phase-dependent evaluation (opening PST vs endgame PST)
 * - Bitboard-based for maximum performance
 */

#ifndef OPERA_HANDCRAFTED_EVAL_H
#define OPERA_HANDCRAFTED_EVAL_H

#include "eval/evaluator_interface.h"
#include "Board.h"
#include "Types.h"
#include <array>
#include <map>
#include <string>

namespace opera {
namespace eval {

/**
 * @class HandcraftedEvaluator
 * @brief Traditional chess evaluator with material + piece-square tables
 *
 * Performance Characteristics:
 * - Target: <1μs per evaluation (competitive requirement)
 * - Method: Bitboard operations with precomputed PST lookups
 * - Optimization: Inline methods, cache-friendly data layout
 *
 * Evaluation Components (Task 3.2 - Foundation):
 * 1. Material balance (P=100, N=320, B=330, R=500, Q=900)
 * 2. Piece-square tables (opening and endgame)
 * 3. Phase detection and tapered evaluation
 * 4. Tempo bonus for side to move
 *
 * Future Extensions (Task 3.3 - Advanced Positional):
 * - Pawn structure analysis (isolated, doubled, passed)
 * - King safety evaluation
 * - Piece mobility
 * - Development bonuses
 */
class HandcraftedEvaluator : public Evaluator {
public:
    /**
     * @brief Construct evaluator with default weights
     */
    HandcraftedEvaluator();

    /**
     * @brief Virtual destructor
     */
    virtual ~HandcraftedEvaluator() = default;

    /**
     * @brief Evaluate position from white's perspective
     *
     * @param board The board position to evaluate
     * @param side_to_move The color whose turn it is to move
     * @return Centipawn score (positive = white advantage, negative = black)
     *
     * @performance Target <1μs average execution time
     */
    int evaluate(const Board& board, Color side_to_move) override;

    /**
     * @brief Configure evaluation parameters from UCI options
     *
     * Supported options:
     * - "MaterialWeight": Multiplier for material evaluation (default 1.0)
     * - "PSTWeight": Multiplier for piece-square table scores (default 1.0)
     * - "TempoBonus": Bonus for side to move (default 10-20cp)
     *
     * @param options Map of option names to values (as strings)
     */
    void configure_options(const std::map<std::string, std::string>& options) override;

protected:
    /**
     * @brief Evaluation configuration weights
     */
    struct EvalWeights {
        // Material values (centipawns)
        static constexpr int PAWN_VALUE = 100;
        static constexpr int KNIGHT_VALUE = 320;
        static constexpr int BISHOP_VALUE = 330;
        static constexpr int ROOK_VALUE = 500;
        static constexpr int QUEEN_VALUE = 900;
        static constexpr int KING_VALUE = 0;  // King is priceless

        // Evaluation component weights
        double material_weight = 1.0;
        double pst_weight = 1.0;
        int tempo_bonus = 15;  // Bonus for side to move
    };

    EvalWeights weights_;

    /**
     * @brief Evaluate material balance for a color
     *
     * @param board The board position
     * @param color The color to evaluate for
     * @return Material score in centipawns
     */
    int evaluate_material(const Board& board, Color color) const;

    /**
     * @brief Evaluate piece-square table score for a color
     *
     * @param board The board position
     * @param color The color to evaluate for
     * @param phase Game phase (0=endgame, 256=opening)
     * @return PST score in centipawns
     */
    int evaluate_pst(const Board& board, Color color, int phase) const;

    /**
     * @brief Calculate game phase based on material
     *
     * Phase calculation:
     * - Opening: All pieces present (phase ≈ 256)
     * - Middlegame: Some pieces traded (phase ≈ 128)
     * - Endgame: Few pieces remaining (phase ≈ 0)
     *
     * @param board The board position
     * @return Phase value (0 = endgame, 256 = opening)
     */
    int calculate_phase(const Board& board) const;

    /**
     * @brief Interpolate between opening and endgame scores
     *
     * Uses tapered evaluation formula:
     * score = (opening_score * phase + endgame_score * (256 - phase)) / 256
     *
     * @param opening_score Score using opening piece-square tables
     * @param endgame_score Score using endgame piece-square tables
     * @param phase Current game phase (0-256)
     * @return Interpolated score
     */
    static int taper_score(int opening_score, int endgame_score, int phase);

    // ========================================================================
    // Piece-Square Tables
    // ========================================================================

    /**
     * @brief Piece-square table for pawns (opening)
     *
     * Emphasizes:
     * - Central pawns (e4, d4, e5, d5)
     * - Pawn chains and support
     * - Discourages edge pawns
     */
    static constexpr std::array<int, 64> PAWN_PST_OPENING = {
         0,  0,  0,  0,  0,  0,  0,  0,  // Rank 1 (shouldn't be here)
        50, 50, 50, 50, 50, 50, 50, 50,  // Rank 2
        10, 10, 20, 30, 30, 20, 10, 10,  // Rank 3
         5,  5, 10, 25, 25, 10,  5,  5,  // Rank 4
         0,  0,  0, 20, 20,  0,  0,  0,  // Rank 5
         5, -5,-10,  0,  0,-10, -5,  5,  // Rank 6
         5, 10, 10,-20,-20, 10, 10,  5,  // Rank 7
         0,  0,  0,  0,  0,  0,  0,  0   // Rank 8
    };

    /**
     * @brief Piece-square table for pawns (endgame)
     *
     * Emphasizes:
     * - Advanced passed pawns
     * - King proximity
     * - Central control
     */
    static constexpr std::array<int, 64> PAWN_PST_ENDGAME = {
          0,  0,  0,  0,  0,  0,  0,  0,  // Rank 1
         80, 80, 80, 80, 80, 80, 80, 80,  // Rank 2 - advanced
         50, 50, 50, 50, 50, 50, 50, 50,  // Rank 3
         30, 30, 30, 30, 30, 30, 30, 30,  // Rank 4
         20, 20, 20, 20, 20, 20, 20, 20,  // Rank 5
         10, 10, 10, 10, 10, 10, 10, 10,  // Rank 6
          5,  5,  5,  5,  5,  5,  5,  5,  // Rank 7
          0,  0,  0,  0,  0,  0,  0,  0   // Rank 8
    };

    /**
     * @brief Piece-square table for knights
     *
     * Emphasizes:
     * - Central outposts
     * - Penalizes rim squares ("knight on rim is dim")
     */
    static constexpr std::array<int, 64> KNIGHT_PST = {
        -50,-40,-30,-30,-30,-30,-40,-50,  // Rank 1 - back rank
        -40,-20,  0,  0,  0,  0,-20,-40,  // Rank 2
        -30,  0, 10, 15, 15, 10,  0,-30,  // Rank 3
        -30,  5, 15, 20, 20, 15,  5,-30,  // Rank 4
        -30,  0, 15, 20, 20, 15,  0,-30,  // Rank 5
        -30,  5, 10, 15, 15, 10,  5,-30,  // Rank 6
        -40,-20,  0,  5,  5,  0,-20,-40,  // Rank 7
        -50,-40,-30,-30,-30,-30,-40,-50   // Rank 8
    };

    /**
     * @brief Piece-square table for bishops
     *
     * Emphasizes:
     * - Long diagonals
     * - Central control
     * - Fianchetto positions
     */
    static constexpr std::array<int, 64> BISHOP_PST = {
        -20,-10,-10,-10,-10,-10,-10,-20,  // Rank 1
        -10,  0,  0,  0,  0,  0,  0,-10,  // Rank 2
        -10,  0,  5, 10, 10,  5,  0,-10,  // Rank 3
        -10,  5,  5, 10, 10,  5,  5,-10,  // Rank 4
        -10,  0, 10, 10, 10, 10,  0,-10,  // Rank 5
        -10, 10, 10, 10, 10, 10, 10,-10,  // Rank 6
        -10,  5,  0,  0,  0,  0,  5,-10,  // Rank 7
        -20,-10,-10,-10,-10,-10,-10,-20   // Rank 8
    };

    /**
     * @brief Piece-square table for rooks
     *
     * Emphasizes:
     * - 7th rank (opponent's back rank area)
     * - Open files
     * - Connected rooks
     */
    static constexpr std::array<int, 64> ROOK_PST = {
          0,  0,  0,  0,  0,  0,  0,  0,  // Rank 1 - white's back rank
          5, 10, 10, 10, 10, 10, 10,  5,  // Rank 2
         -5,  0,  0,  0,  0,  0,  0, -5,  // Rank 3
         -5,  0,  0,  0,  0,  0,  0, -5,  // Rank 4
         -5,  0,  0,  0,  0,  0,  0, -5,  // Rank 5
         -5,  0,  0,  0,  0,  0,  0, -5,  // Rank 6
          5, 10, 10, 10, 10, 10, 10,  5,  // Rank 7 - opponent's 2nd rank (GOOD!)
          0,  0,  0,  5,  5,  0,  0,  0   // Rank 8 - opponent's back rank
    };

    /**
     * @brief Piece-square table for queens
     *
     * Emphasizes:
     * - Central control
     * - Avoid early development
     */
    static constexpr std::array<int, 64> QUEEN_PST = {
        -20,-10,-10, -5, -5,-10,-10,-20,  // Rank 1
        -10,  0,  0,  0,  0,  0,  0,-10,  // Rank 2
        -10,  0,  5,  5,  5,  5,  0,-10,  // Rank 3
         -5,  0,  5,  5,  5,  5,  0, -5,  // Rank 4
          0,  0,  5,  5,  5,  5,  0, -5,  // Rank 5
        -10,  5,  5,  5,  5,  5,  0,-10,  // Rank 6
        -10,  0,  5,  0,  0,  0,  0,-10,  // Rank 7
        -20,-10,-10, -5, -5,-10,-10,-20   // Rank 8
    };

    /**
     * @brief Piece-square table for king (opening/middlegame)
     *
     * Emphasizes:
     * - Castled position
     * - Pawn shield
     * - Avoid center
     */
    static constexpr std::array<int, 64> KING_PST_OPENING = {
        -30,-40,-40,-50,-50,-40,-40,-30,  // Rank 1
        -30,-40,-40,-50,-50,-40,-40,-30,  // Rank 2
        -30,-40,-40,-50,-50,-40,-40,-30,  // Rank 3
        -30,-40,-40,-50,-50,-40,-40,-30,  // Rank 4
        -20,-30,-30,-40,-40,-30,-30,-20,  // Rank 5
        -10,-20,-20,-20,-20,-20,-20,-10,  // Rank 6
         20, 20,  0,  0,  0,  0, 20, 20,  // Rank 7 - castled
         20, 30, 10,  0,  0, 10, 30, 20   // Rank 8 - castled
    };

    /**
     * @brief Piece-square table for king (endgame)
     *
     * Emphasizes:
     * - Centralization
     * - Activity
     * - Pawn endgame support
     */
    static constexpr std::array<int, 64> KING_PST_ENDGAME = {
        -50,-40,-30,-20,-20,-30,-40,-50,  // Rank 1
        -30,-20,-10,  0,  0,-10,-20,-30,  // Rank 2
        -30,-10, 20, 30, 30, 20,-10,-30,  // Rank 3
        -30,-10, 30, 40, 40, 30,-10,-30,  // Rank 4
        -30,-10, 30, 40, 40, 30,-10,-30,  // Rank 5
        -30,-10, 20, 30, 30, 20,-10,-30,  // Rank 6
        -30,-30,  0,  0,  0,  0,-30,-30,  // Rank 7
        -50,-30,-30,-30,-30,-30,-30,-50   // Rank 8
    };

    /**
     * @brief Get PST value for a piece on a square
     *
     * @param piece_type The type of piece
     * @param square The square (0-63)
     * @param color The piece color (for flipping table for black)
     * @param phase Game phase (for tapered king PST)
     * @return PST bonus in centipawns
     */
    int get_pst_value(PieceType piece_type, Square square, Color color, int phase) const;

    /**
     * @brief Flip square for black piece-square table lookup
     *
     * Black pieces use same tables but from opposite perspective.
     * E.g., black pawn on rank 7 is like white pawn on rank 2.
     *
     * @param square Original square (0-63)
     * @return Flipped square
     */
    static constexpr Square flip_square(Square square) {
        return static_cast<Square>(square ^ 56);  // XOR with 56 flips ranks
    }
};

} // namespace eval
} // namespace opera

#endif // OPERA_HANDCRAFTED_EVAL_H
