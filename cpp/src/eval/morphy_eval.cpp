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
    EvaluationTerms terms;
    int base_score = evaluate_with_terms(board, side_to_move, terms, morphy_bias_ >= 0.01);

    // If Morphy bias is 0, just return base score (normal play)
    if (morphy_bias_ < 0.01) {
        return base_score;
    }

    // Calculate phase for context-dependent adjustments
    int phase = terms.phase;

    // Get component scores for white and black
    Color white = Color::WHITE;
    Color black = Color::BLACK;

    // ========================================================================
    // Apply Morphy-specific biases
    // ========================================================================

    int morphy_adjustment = 0;

    // Reward participation by additional pieces, not repeated activity by one
    // knight. These opening features fade out before the endgame.
    morphy_adjustment += static_cast<int>(morphy_bias_ *
        (development_activity(board, WHITE, phase, terms.activity[WHITE]) -
         development_activity(board, BLACK, phase, terms.activity[BLACK])));

    // 2. King Safety Aggression Bias (1.5x for attacking enemy king)
    {
        int white_king_safety = terms.king_safety[WHITE];
        int black_king_safety = terms.king_safety[BLACK];

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
        int white_mobility = terms.mobility[WHITE];
        int black_mobility = terms.mobility[BLACK];
        int mobility_advantage = white_mobility - black_mobility;

        double mobility_multiplier = 1.0 + (MOBILITY_BIAS - 1.0) * morphy_bias_;
        int mobility_bonus = static_cast<int>(mobility_advantage * (mobility_multiplier - 1.0));
        morphy_adjustment += mobility_bonus;
    }

    // 4. Uncastled King Penalty (Morphy-specific)
    if (phase > 0) {  // Opening/middlegame only
        if (is_uncastled_in_opening(board, black, phase)) {
            int penalty = static_cast<int>(UNCASTLED_PENALTY * morphy_bias_ * phase / 256.0);
            morphy_adjustment += penalty;  // Good for white
        }
        if (is_uncastled_in_opening(board, white, phase)) {
            int penalty = static_cast<int>(UNCASTLED_PENALTY * morphy_bias_ * phase / 256.0);
            morphy_adjustment -= penalty;  // Bad for white
        }
    }

    // 5. Material Sacrifice Compensation
    {
        int white_material = terms.material[WHITE];
        int black_material = terms.material[BLACK];
        int material_balance = white_material - black_material;

        // If white is behind in material, check for compensation
        if (material_balance < -50) {  // Down at least half a pawn
            int compensation = calculate_sacrifice_compensation(board, white, material_balance, terms);
            morphy_adjustment += compensation;
        }
        // If black is behind, check their compensation (subtract from white's score)
        else if (material_balance > 50) {
            int compensation = calculate_sacrifice_compensation(board, black, -material_balance, terms);
            morphy_adjustment -= compensation;
        }
    }

    // Value activity even before material is sacrificed; compensation must be
    // earned by a real advantage over the opponent, not merely by losing a pawn.
    morphy_adjustment += static_cast<int>(morphy_bias_ *
        (calculate_initiative(board, WHITE, terms) - calculate_initiative(board, BLACK, terms)) / 4.0);
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

int MorphyEvaluator::development_activity(const Board& board, Color color, int phase, const MobilityDetails& activity) const {
    const int opening = std::clamp((phase - 128) * 2, 0, 256);
    if (!opening) return 0;
    const Bitboard occupied = board.getOccupiedBitboard();
    const Bitboard pawns = board.getPieceBitboard(color, PAWN);
    const Bitboard enemy_pawns = board.getPieceBitboard(~color, PAWN);
    const Bitboard home = color == WHITE ? 0xffULL : 0xff00000000000000ULL;
    const Bitboard minors = board.getPieceBitboard(color, KNIGHT) | board.getPieceBitboard(color, BISHOP);
    const int waiting = __builtin_popcountll(minors & home);
    auto pawn_attacks = [](Bitboard p, Color c) {
        constexpr Bitboard not_a = 0xfefefefefefefefeULL, not_h = 0x7f7f7f7f7f7f7f7fULL;
        return c == WHITE ? ((p & not_a) << 7) | ((p & not_h) << 9)
                          : ((p & not_a) >> 9) | ((p & not_h) >> 7);
    };
    const Bitboard enemy_attacks = pawn_attacks(enemy_pawns, ~color);
    // One legal-shaped pawn push can gain a tempo. Exclude occupied and
    // pawn-controlled destinations; pins and tactical exceptions remain search's job.
    Bitboard pushes = (color == WHITE ? enemy_pawns >> 8 : enemy_pawns << 8) & ~occupied;
    const Bitboard intermediate = pushes & (color == WHITE ? 0x0000ff0000000000ULL : 0x0000000000ff0000ULL);
    pushes |= (color == WHITE ? intermediate >> 8 : intermediate << 8) & ~occupied;
    pushes &= ~pawn_attacks(pawns, color);
    const Bitboard pawn_kicks = pawn_attacks(pushes, ~color);
    constexpr Bitboard centre = 0x0000001818000000ULL;
    int developed = __builtin_popcountll(minors & ~home);
    int score = 44 * developed + 4 * activity.minor_safe_squares + 4 * activity.minor_centre_control;
    score += 12 * __builtin_popcountll(minors & centre);
    score += 12 * __builtin_popcountll(board.getPieceBitboard(color, BISHOP) & ~home);
    score -= 24 * __builtin_popcountll(minors & ~home & enemy_attacks);
    score -= 10 * waiting * __builtin_popcountll(board.getPieceBitboard(color, KNIGHT) & ~home & pawn_kicks);
    // Knights can move immediately; bishops need a pawn move first. Without
    // this readiness cost, larger development bonuses exaggerate knight-only play.
    score += 6 * activity.bishop_exits - 48 * activity.blocked_bishops;
    developed = std::min(4, developed);
    score += 2 * developed * (developed - 1);
    // Central footholds free the bishops and contest space. Further pawn
    // advances do not keep accumulating development credit.
    Bitboard central_pawns = pawns & 0x1818181818181818ULL;
    while (central_pawns) {
        const int sq = __builtin_ctzll(central_pawns);
        central_pawns &= central_pawns - 1;
        const int rank = color == WHITE ? sq / 8 : 7 - sq / 8;
        if (rank == 2) score += 20;              // Opens a bishop diagonal.
        if (rank == 3 || rank == 4) score += 50; // Occupies d4/e4/d5/e5.
    }
    if (board.getPieceBitboard(color, QUEEN) & ~home) score -= 8 * waiting;
    score += 18 * activity.connected_rook_pairs;
    return score * opening / 256;
}

int MorphyEvaluator::calculate_sacrifice_compensation(
    const Board& board, Color color, int material_deficit, const EvaluationTerms& terms) const {

    // No compensation for large material deficits (>400cp = more than a minor piece)
    if (material_deficit < -400) {
        return 0;
    }

    int compensation = 0;

    // Calculate initiative advantage
    int initiative = std::max(0, calculate_initiative(board, color, terms) - calculate_initiative(board, ~color, terms));
    compensation += initiative;

    // Check for king attack potential (enemy king safety)
    Color enemy = ~color;
    int phase = terms.phase;
    int enemy_king_safety = terms.king_safety[enemy];

    // Poor enemy king safety = compensation for sacrifice
    if (enemy_king_safety < -20 && enemy_king_safety < terms.king_safety[color] - 20) {  // Enemy king is unsafe
        compensation += std::min(30, -enemy_king_safety);
    }

    // Check for development advantage
    if (phase > 0) {  // Opening/middlegame
        int our_dev = terms.development[color];
        int enemy_dev = terms.development[enemy];
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
    if (phase == 0) {
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

    // Both castling wings are safe starting points; walking the king forward
    // does not make the opening penalty disappear.
    const int relative_rank = enemy_color == WHITE ? king_rank : 7 - king_rank;
    return relative_rank > 1 || (king_file >= 3 && king_file <= 5);

}

int MorphyEvaluator::calculate_initiative(const Board& board, Color color, const EvaluationTerms& terms) const {
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
    int mobility = terms.mobility[color];
    initiative += mobility / 3;  // Use 1/3 of mobility score

    // 3. Development in opening
    int phase = terms.phase;
    if (phase > 0) {
        int development = terms.development[color];
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
