/**
 * @file morphy_eval.cpp
 * @brief Implementation of Morphy-style chess evaluator
 *
 * Applies Paul Morphy's playing characteristics through bias multipliers
 * on top of the base HandcraftedEvaluator.
 */

#include "eval/morphy_eval.h"
#include "Board.h"
#include "Types.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace opera {
namespace eval {

MorphyEvaluator::MorphyEvaluator(double morphy_bias)
    : HandcraftedEvaluator(), morphy_bias_(morphy_bias) {
    // Clamp bias to valid range [0.0, 2.0]
    morphy_bias_ = std::max(0.0, std::min(2.0, morphy_bias_));
}

int MorphyEvaluator::evaluate(const Board& board, Color side_to_move) {
    // Start with base handcrafted evaluation
    int base_score = HandcraftedEvaluator::evaluate(board, side_to_move);

    // If Morphy bias is 0, just return base score (normal play)
    if (morphy_bias_ < 0.01) {
        return base_score;
    }

    // Calculate phase for context-dependent adjustments
    int phase = calculate_phase(board);

    // Get component scores for white and black
    Color white = Color::WHITE;
    Color black = Color::BLACK;

    // ========================================================================
    // Apply Morphy-specific biases
    // ========================================================================

    int morphy_adjustment = 0;

    // 1. Development Bias (1.2x in opening, fades in endgame)
    if (phase > 128) {  // Opening/middlegame
        int white_dev = evaluate_development(board, white, phase);
        int black_dev = evaluate_development(board, black, phase);
        int dev_advantage = white_dev - black_dev;

        // Apply development bias (extra 0.2x scaled by morphy_bias)
        double dev_multiplier = 1.0 + (DEVELOPMENT_BIAS - 1.0) * morphy_bias_;
        int dev_bonus = static_cast<int>(dev_advantage * (dev_multiplier - 1.0));
        morphy_adjustment += dev_bonus;
    }

    // 2. King Safety Aggression Bias (1.5x for attacking enemy king)
    {
        int white_king_safety = evaluate_king_safety(board, white, phase);
        int black_king_safety = evaluate_king_safety(board, black, phase);

        // Morphy focuses on ATTACKING enemy king (black's safety matters more)
        // Negative black king safety = good for white
        double king_multiplier = 1.0 + (KING_SAFETY_BIAS - 1.0) * morphy_bias_;

        // Apply extra penalty to enemy king being unsafe
        int white_king_attack_bonus = static_cast<int>(-black_king_safety * (king_multiplier - 1.0));
        int black_king_attack_bonus = static_cast<int>(-white_king_safety * (king_multiplier - 1.0));
        morphy_adjustment += (white_king_attack_bonus - black_king_attack_bonus);
    }

    // 3. Mobility and Initiative Bias (1.1x for piece activity)
    {
        int white_mobility = evaluate_mobility(board, white);
        int black_mobility = evaluate_mobility(board, black);
        int mobility_advantage = white_mobility - black_mobility;

        double mobility_multiplier = 1.0 + (MOBILITY_BIAS - 1.0) * morphy_bias_;
        int mobility_bonus = static_cast<int>(mobility_advantage * (mobility_multiplier - 1.0));
        morphy_adjustment += mobility_bonus;
    }

    // 4. Uncastled King Penalty (Morphy-specific)
    if (phase > 128) {  // Opening/middlegame only
        if (is_uncastled_in_opening(board, black, phase)) {
            int penalty = static_cast<int>(UNCASTLED_PENALTY * morphy_bias_);
            morphy_adjustment += penalty;  // Good for white
        }
        if (is_uncastled_in_opening(board, white, phase)) {
            int penalty = static_cast<int>(UNCASTLED_PENALTY * morphy_bias_);
            morphy_adjustment -= penalty;  // Bad for white
        }
    }

    // 5. Material Sacrifice Compensation
    {
        int white_material = evaluate_material(board, white);
        int black_material = evaluate_material(board, black);
        int material_balance = white_material - black_material;

        // If white is behind in material, check for compensation
        if (material_balance < -50) {  // Down at least half a pawn
            int compensation = calculate_sacrifice_compensation(board, white, material_balance);
            morphy_adjustment += compensation;
        }
        // If black is behind, check their compensation (subtract from white's score)
        else if (material_balance > 50) {
            int compensation = calculate_sacrifice_compensation(board, black, -material_balance);
            morphy_adjustment -= compensation;
        }
    }

    return base_score + morphy_adjustment;
}

void MorphyEvaluator::configure_options(const std::map<std::string, std::string>& options) {
    // First configure base evaluator options
    HandcraftedEvaluator::configure_options(options);

    // Then handle Morphy-specific options
    auto it = options.find("MorphyBias");
    if (it != options.end()) {
        double bias = std::atof(it->second.c_str());
        morphy_bias_ = std::max(0.0, std::min(2.0, bias));  // Clamp to [0.0, 2.0]
    }
}

// ============================================================================
// Private Helper Methods
// ============================================================================

int MorphyEvaluator::calculate_sacrifice_compensation(
    const Board& board, Color color, int material_deficit) const {

    // No compensation for large material deficits (>400cp = more than a minor piece)
    if (material_deficit < -400) {
        return 0;
    }

    int compensation = 0;

    // Calculate initiative advantage
    int initiative = calculate_initiative(board, color);
    compensation += initiative;

    // Check for king attack potential (enemy king safety)
    Color enemy = ~color;
    int phase = calculate_phase(board);
    int enemy_king_safety = evaluate_king_safety(board, enemy, phase);

    // Poor enemy king safety = compensation for sacrifice
    if (enemy_king_safety < -20) {  // Enemy king is unsafe
        compensation += std::min(30, -enemy_king_safety);
    }

    // Check for development advantage
    if (phase > 128) {  // Opening/middlegame
        int our_dev = evaluate_development(board, color, phase);
        int enemy_dev = evaluate_development(board, enemy, phase);
        if (our_dev > enemy_dev + 20) {  // Significant development lead
            compensation += 20;
        }
    }

    // Scale compensation by morphy_bias and cap at SACRIFICE_COMPENSATION
    compensation = static_cast<int>(compensation * morphy_bias_);
    compensation = std::min(SACRIFICE_COMPENSATION, compensation);

    return compensation;
}

bool MorphyEvaluator::is_uncastled_in_opening(
    const Board& board, Color enemy_color, int phase) const {

    // Only check in opening/middlegame
    if (phase < 128) {
        return false;
    }

    // Get enemy king position
    uint64_t king_bb = board.getPieceBitboard(enemy_color, KING);
    if (king_bb == 0) {
        return false;  // No king (shouldn't happen)
    }

    Square king_sq = static_cast<Square>(__builtin_ctzll(king_bb));
    int king_file = king_sq % 8;
    int king_rank = king_sq / 8;

    // King on back rank in center (files c-f) = not castled
    int back_rank = (enemy_color == Color::WHITE) ? 0 : 7;
    if (king_rank == back_rank && king_file >= 2 && king_file <= 5) {
        return true;  // King still in center on back rank
    }

    return false;
}

int MorphyEvaluator::calculate_initiative(const Board& board, Color color) const {
    int initiative = 0;

    // 1. Central control
    uint64_t center = 0x0000001818000000ULL;  // e4, d4, e5, d5
    uint64_t our_pieces = 0;
    for (int pt = PAWN; pt <= QUEEN; ++pt) {
        our_pieces |= board.getPieceBitboard(color, static_cast<PieceType>(pt));
    }
    int central_pieces = __builtin_popcountll(our_pieces & center);
    initiative += central_pieces * 5;  // 5cp per central piece

    // 2. Piece mobility advantage
    int mobility = evaluate_mobility(board, color);
    initiative += mobility / 3;  // Use 1/3 of mobility score

    // 3. Development in opening
    int phase = calculate_phase(board);
    if (phase > 128) {
        int development = evaluate_development(board, color, phase);
        initiative += development / 4;  // Use 1/4 of development score
    }

    // 4. Active rooks (on open/semi-open files)
    uint64_t rooks = board.getPieceBitboard(color, ROOK);
    uint64_t our_pawns = board.getPieceBitboard(color, PAWN);
    int active_rooks = 0;

    uint64_t rook_bb = rooks;
    while (rook_bb) {
        Square sq = static_cast<Square>(__builtin_ctzll(rook_bb));
        int file = sq % 8;

        uint64_t file_mask = 0x0101010101010101ULL << file;
        bool has_pawn = (our_pawns & file_mask) != 0;

        if (!has_pawn) {
            active_rooks++;  // Rook on semi-open or open file
        }

        rook_bb &= rook_bb - 1;
    }
    initiative += active_rooks * 10;  // 10cp per active rook

    return initiative;
}

} // namespace eval
} // namespace opera
