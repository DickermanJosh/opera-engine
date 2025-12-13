/**
 * @file handcrafted_eval.cpp
 * @brief Implementation of traditional handcrafted chess evaluation
 *
 * Performance-optimized implementation using:
 * - Bitboard operations for piece counting
 * - Precomputed piece-square table lookups
 * - Inline methods for hot code paths
 * - Cache-friendly data access patterns
 *
 * Target: <1μs per evaluation on modern hardware
 */

#include "eval/handcrafted_eval.h"
#include "Board.h"
#include "Types.h"
#include <algorithm>
#include <cstdlib>  // For atoi, atof

namespace opera {
namespace eval {

// ============================================================================
// Constructor
// ============================================================================

HandcraftedEvaluator::HandcraftedEvaluator()
    : weights_()
{
    // Default weights initialized in struct definition
}

// ============================================================================
// Main Evaluation Function
// ============================================================================

int HandcraftedEvaluator::evaluate(const Board& board, Color side_to_move) {
    // Calculate game phase for tapered evaluation
    int phase = calculate_phase(board);

    // Evaluate material for both sides
    int white_material = evaluate_material(board, Color::WHITE);
    int black_material = evaluate_material(board, Color::BLACK);

    // Evaluate piece-square tables for both sides
    int white_pst = evaluate_pst(board, Color::WHITE, phase);
    int black_pst = evaluate_pst(board, Color::BLACK, phase);

    // Combine evaluations (from white's perspective)
    int material_score = white_material - black_material;
    int pst_score = white_pst - black_pst;

    // Apply weights
    int total_score = static_cast<int>(
        material_score * weights_.material_weight +
        pst_score * weights_.pst_weight
    );

    // Add tempo bonus for side to move (only if there's material on board)
    int total_material = white_material + black_material;
    if (total_material > 0) {
        if (side_to_move == Color::WHITE) {
            total_score += weights_.tempo_bonus;
        } else {
            total_score -= weights_.tempo_bonus;
        }
    }

    return total_score;
}

// ============================================================================
// Configuration
// ============================================================================

void HandcraftedEvaluator::configure_options(
    const std::map<std::string, std::string>& options)
{
    // Parse configuration options
    auto it = options.find("MaterialWeight");
    if (it != options.end()) {
        weights_.material_weight = std::atof(it->second.c_str());
    }

    it = options.find("PSTWeight");
    if (it != options.end()) {
        weights_.pst_weight = std::atof(it->second.c_str());
    }

    it = options.find("TempoBonus");
    if (it != options.end()) {
        weights_.tempo_bonus = std::atoi(it->second.c_str());
    }
}

// ============================================================================
// Material Evaluation
// ============================================================================

int HandcraftedEvaluator::evaluate_material(const Board& board, Color color) const {
    int material = 0;

    // Get piece bitboards for this color
    // Note: Board interface provides getPieceBitboard(Color, PieceType)

    // Count pawns
    uint64_t pawns = board.getPieceBitboard(color, PAWN);
    material += __builtin_popcountll(pawns) * EvalWeights::PAWN_VALUE;

    // Count knights
    uint64_t knights = board.getPieceBitboard(color, KNIGHT);
    material += __builtin_popcountll(knights) * EvalWeights::KNIGHT_VALUE;

    // Count bishops
    uint64_t bishops = board.getPieceBitboard(color, BISHOP);
    material += __builtin_popcountll(bishops) * EvalWeights::BISHOP_VALUE;

    // Count rooks
    uint64_t rooks = board.getPieceBitboard(color, ROOK);
    material += __builtin_popcountll(rooks) * EvalWeights::ROOK_VALUE;

    // Count queens
    uint64_t queens = board.getPieceBitboard(color, QUEEN);
    material += __builtin_popcountll(queens) * EvalWeights::QUEEN_VALUE;

    return material;
}

// ============================================================================
// Piece-Square Table Evaluation
// ============================================================================

int HandcraftedEvaluator::evaluate_pst(const Board& board, Color color, int phase) const {
    int pst_score = 0;

    // Evaluate each piece type
    // Pawns
    uint64_t pawns = board.getPieceBitboard(color, PAWN);
    while (pawns) {
        Square sq = static_cast<Square>(__builtin_ctzll(pawns));
        pst_score += get_pst_value(PAWN, sq, color, phase);
        pawns &= pawns - 1;  // Clear lowest bit
    }

    // Knights
    uint64_t knights = board.getPieceBitboard(color, KNIGHT);
    while (knights) {
        Square sq = static_cast<Square>(__builtin_ctzll(knights));
        pst_score += get_pst_value(KNIGHT, sq, color, phase);
        knights &= knights - 1;
    }

    // Bishops
    uint64_t bishops = board.getPieceBitboard(color, BISHOP);
    while (bishops) {
        Square sq = static_cast<Square>(__builtin_ctzll(bishops));
        pst_score += get_pst_value(BISHOP, sq, color, phase);
        bishops &= bishops - 1;
    }

    // Rooks
    uint64_t rooks = board.getPieceBitboard(color, ROOK);
    while (rooks) {
        Square sq = static_cast<Square>(__builtin_ctzll(rooks));
        pst_score += get_pst_value(ROOK, sq, color, phase);
        rooks &= rooks - 1;
    }

    // Queens
    uint64_t queens = board.getPieceBitboard(color, QUEEN);
    while (queens) {
        Square sq = static_cast<Square>(__builtin_ctzll(queens));
        pst_score += get_pst_value(QUEEN, sq, color, phase);
        queens &= queens - 1;
    }

    // King
    uint64_t king = board.getPieceBitboard(color, KING);
    if (king) {
        Square sq = static_cast<Square>(__builtin_ctzll(king));
        pst_score += get_pst_value(KING, sq, color, phase);
    }

    return pst_score;
}

// ============================================================================
// Phase Calculation
// ============================================================================

int HandcraftedEvaluator::calculate_phase(const Board& board) const {
    // Phase is based on remaining material
    // Opening: All pieces present (phase ≈ 256)
    // Endgame: Few pieces (phase ≈ 0)

    int phase = 0;

    // Count non-pawn, non-king pieces for both sides
    // Knight/Bishop = 1 phase point
    // Rook = 2 phase points
    // Queen = 4 phase points

    for (Color color : {Color::WHITE, Color::BLACK}) {
        phase += __builtin_popcountll(board.getPieceBitboard(color, KNIGHT)) * 1;
        phase += __builtin_popcountll(board.getPieceBitboard(color, BISHOP)) * 1;
        phase += __builtin_popcountll(board.getPieceBitboard(color, ROOK)) * 2;
        phase += __builtin_popcountll(board.getPieceBitboard(color, QUEEN)) * 4;
    }

    // Starting position has 24 phase points (4N + 4B + 4R + 2Q)
    // Scale to 0-256 range
    // 24 points = 256 (opening)
    // 0 points = 0 (endgame)

    phase = (phase * 256 + 12) / 24;  // +12 for rounding

    // Clamp to valid range
    if (phase > 256) phase = 256;
    if (phase < 0) phase = 0;

    return phase;
}

// ============================================================================
// Tapered Evaluation
// ============================================================================

int HandcraftedEvaluator::taper_score(int opening_score, int endgame_score, int phase) {
    // Linear interpolation between opening and endgame scores
    // phase = 256 → full opening score
    // phase = 0 → full endgame score
    return (opening_score * phase + endgame_score * (256 - phase)) / 256;
}

// ============================================================================
// Piece-Square Table Lookup
// ============================================================================

int HandcraftedEvaluator::get_pst_value(
    PieceType piece_type, Square square, Color color, int phase) const
{
    // Flip square for black pieces (they use same tables from opposite perspective)
    Square table_square = (color == Color::BLACK) ? flip_square(square) : square;

    switch (piece_type) {
        case PAWN: {
            int opening_value = PAWN_PST_OPENING[table_square];
            int endgame_value = PAWN_PST_ENDGAME[table_square];
            return taper_score(opening_value, endgame_value, phase);
        }

        case KNIGHT:
            return KNIGHT_PST[table_square];

        case BISHOP:
            return BISHOP_PST[table_square];

        case ROOK:
            return ROOK_PST[table_square];

        case QUEEN:
            return QUEEN_PST[table_square];

        case KING: {
            int opening_value = KING_PST_OPENING[table_square];
            int endgame_value = KING_PST_ENDGAME[table_square];
            return taper_score(opening_value, endgame_value, phase);
        }

        default:
            return 0;
    }
}

} // namespace eval
} // namespace opera
