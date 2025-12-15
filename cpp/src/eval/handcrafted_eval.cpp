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
    : weights_(), pawn_hash_stats_()
{
    // Initialize pawn hash table
    size_t num_entries = (pawn_hash_size_mb_ * 1024 * 1024) / PAWN_HASH_ENTRY_SIZE;
    pawn_hash_table_.resize(num_entries);
    clear_pawn_hash();
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

    // Advanced positional evaluation (Task 3.3)
    // Pawn structure with caching (Task 3.6)
    int white_pawn_structure = 0;
    int black_pawn_structure = 0;

    uint64_t pawn_key = calculate_pawn_key(board);
    PawnHashEntry pawn_entry;

    if (probe_pawn_hash(pawn_key, pawn_entry)) {
        // Cache hit - use stored pawn structure evaluation
        // Taper between middlegame and endgame scores
        white_pawn_structure = (pawn_entry.score_mg * phase + pawn_entry.score_eg * (256 - phase)) / 256;
        // For now, assume symmetric storage (white - black in single score)
        // TODO: Store white and black separately if needed
        black_pawn_structure = 0;  // Already included in white_pawn_structure as differential
    } else {
        // Cache miss - compute pawn structure
        white_pawn_structure = evaluate_pawn_structure(board, Color::WHITE);
        black_pawn_structure = evaluate_pawn_structure(board, Color::BLACK);

        // Store in pawn hash (using differential score for now)
        int pawn_score_diff = white_pawn_structure - black_pawn_structure;
        store_pawn_hash(pawn_key, pawn_score_diff, pawn_score_diff, 0, 0, 0);
    }

    int white_king_safety = evaluate_king_safety(board, Color::WHITE, phase);
    int black_king_safety = evaluate_king_safety(board, Color::BLACK, phase);

    int white_mobility = evaluate_mobility(board, Color::WHITE);
    int black_mobility = evaluate_mobility(board, Color::BLACK);

    int white_development = evaluate_development(board, Color::WHITE, phase);
    int black_development = evaluate_development(board, Color::BLACK, phase);

    // Combine evaluations (from white's perspective)
    int material_score = white_material - black_material;
    int pst_score = white_pst - black_pst;
    int pawn_structure_score = white_pawn_structure - black_pawn_structure;
    int king_safety_score = white_king_safety - black_king_safety;
    int mobility_score = white_mobility - black_mobility;
    int development_score = white_development - black_development;

    // Apply weights
    int total_score = static_cast<int>(
        material_score * weights_.material_weight +
        pst_score * weights_.pst_weight +
        pawn_structure_score * weights_.pawn_structure_weight +
        king_safety_score * weights_.king_safety_weight +
        mobility_score * weights_.mobility_weight +
        development_score * weights_.development_weight
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

    // Pawn hash table size configuration (Task 3.6)
    it = options.find("PawnHashSize");
    if (it != options.end()) {
        size_t new_size_mb = std::atoi(it->second.c_str());
        if (new_size_mb != pawn_hash_size_mb_ && new_size_mb > 0 && new_size_mb <= 256) {
            pawn_hash_size_mb_ = new_size_mb;
            size_t num_entries = (pawn_hash_size_mb_ * 1024 * 1024) / PAWN_HASH_ENTRY_SIZE;
            pawn_hash_table_.resize(num_entries);
            clear_pawn_hash();
        }
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

// ============================================================================
// Advanced Positional Evaluation - Task 3.3
// ============================================================================

// Helper Methods for Pawn Structure Analysis

bool HandcraftedEvaluator::is_isolated_pawn(uint64_t pawns, Square square) const {
    int file = square % 8;
    uint64_t adjacent_files = adjacent_files_mask(file);
    return (pawns & adjacent_files) == 0;
}

bool HandcraftedEvaluator::is_passed_pawn(uint64_t enemy_pawns, Square square, Color color) const {
    uint64_t forward_span = forward_span_mask(square, color);
    return (enemy_pawns & forward_span) == 0;
}

// count_pawns_on_file removed - functionality is inline in evaluate_pawn_structure

uint64_t HandcraftedEvaluator::forward_span_mask(Square square, Color color) const {
    int file = square % 8;
    int rank = square / 8;

    uint64_t mask = 0;

    // Include same file and adjacent files
    uint64_t files = file_mask(file) | adjacent_files_mask(file);

    // Include all ranks ahead of this pawn
    if (color == Color::WHITE) {
        // White pawns move up (increasing rank)
        for (int r = rank + 1; r < 8; ++r) {
            mask |= files & (0xFFULL << (r * 8));
        }
    } else {
        // Black pawns move down (decreasing rank)
        for (int r = rank - 1; r >= 0; --r) {
            mask |= files & (0xFFULL << (r * 8));
        }
    }

    return mask;
}

// Pawn Structure Evaluation

int HandcraftedEvaluator::evaluate_pawn_structure(const Board& board, Color color) const {
    int score = 0;

    uint64_t pawns = board.getPieceBitboard(color, PAWN);
    uint64_t enemy_pawns = board.getPieceBitboard(~color, PAWN);

    // Track files with pawns for doubled pawn detection
    int file_counts[8] = {0};

    uint64_t pawn_bb = pawns;
    while (pawn_bb) {
        Square sq = static_cast<Square>(__builtin_ctzll(pawn_bb));
        int file = sq % 8;
        int rank = sq / 8;

        // Count pawns on this file
        file_counts[file]++;

        // Check for isolated pawn
        if (is_isolated_pawn(pawns, sq)) {
            score -= weights_.isolated_pawn_penalty;
        }

        // Check for passed pawn
        if (is_passed_pawn(enemy_pawns, sq, color)) {
            // Bonus scales with rank (closer to promotion = higher bonus)
            int pawn_rank = (color == Color::WHITE) ? rank : (7 - rank);
            score += EvalWeights::PASSED_PAWN_BONUS[pawn_rank];
        }

        pawn_bb &= pawn_bb - 1;  // Clear lowest bit
    }

    // Apply doubled pawn penalties
    for (int file = 0; file < 8; ++file) {
        if (file_counts[file] > 1) {
            // Penalty for each doubled pawn (beyond the first)
            score -= weights_.doubled_pawn_penalty * (file_counts[file] - 1);
        }
    }

    return score;
}

// King Safety Evaluation

int HandcraftedEvaluator::evaluate_king_safety(const Board& board, Color color, int phase) const {
    int score = 0;

    uint64_t king_bb = board.getPieceBitboard(color, KING);
    if (!king_bb) return 0;

    Square king_sq = static_cast<Square>(__builtin_ctzll(king_bb));
    int king_file = king_sq % 8;
    // int king_rank = king_sq / 8;  // Unused for now

    // King safety is more important in opening (high phase)
    // In endgame (low phase), king centralization is already handled by PST
    // Note: Don't hard-disable for low phase - taper at the end instead

    uint64_t friendly_pawns = board.getPieceBitboard(color, PAWN);

    // Evaluate pawn shield (pawns in front of king)
    // For white: check f2/g2/h2 if king on g1 (kingside castle)
    // For black: check f7/g7/h7 if king on g8 (kingside castle)

    int shield_score = 0;
    int open_file_penalty = 0;

    // Check files around king (king_file - 1, king_file, king_file + 1)
    for (int file_offset = -1; file_offset <= 1; ++file_offset) {
        int file = king_file + file_offset;
        if (file < 0 || file > 7) continue;

        uint64_t file_pawns = friendly_pawns & file_mask(file);

        if (file_pawns == 0) {
            // Open file near king - dangerous
            open_file_penalty += weights_.open_file_near_king_penalty;
        } else {
            // Check if pawn shield is intact (on 2nd/3rd rank for white, 6th/7th for black)
            // Only count pawns close to king, not ones that have advanced far away
            int shield_rank_1 = (color == Color::WHITE) ? 1 : 6;  // 2nd/7th rank
            int shield_rank_2 = (color == Color::WHITE) ? 2 : 5;  // 3rd/6th rank
            int too_advanced = (color == Color::WHITE) ? 4 : 3;   // 5th/4th rank or beyond

            bool has_shield = false;
            bool too_far_advanced = false;

            uint64_t temp_pawns = file_pawns;
            while (temp_pawns) {
                Square pawn_sq = static_cast<Square>(__builtin_ctzll(temp_pawns));
                int pawn_rank = pawn_sq / 8;

                if (pawn_rank == shield_rank_1 || pawn_rank == shield_rank_2) {
                    has_shield = true;
                } else if (color == Color::WHITE ? (pawn_rank >= too_advanced) : (pawn_rank <= too_advanced)) {
                    too_far_advanced = true;
                }

                temp_pawns &= temp_pawns - 1;
            }

            if (has_shield) {
                // Pawn on shield ranks (2nd/3rd or 6th/7th) - good
                shield_score += weights_.pawn_shield_bonus;
            } else if (too_far_advanced) {
                // Pawn advanced too far - file is essentially open near king
                open_file_penalty += weights_.open_file_near_king_penalty / 2;
            }
        }
    }

    score += shield_score;
    score -= open_file_penalty;

    // Scale king safety by phase (more important in opening)
    score = (score * phase) / 256;

    return score;
}

// Piece Mobility Evaluation

int HandcraftedEvaluator::evaluate_mobility(const Board& board, Color color) const {
    int score = 0;

    // This is a simplified mobility calculation
    // Full mobility would require generating pseudo-legal moves for each piece
    // For now, we'll use piece positioning and bitboard analysis

    // Knight mobility: knights in center have more squares
    // (Already captured by PST, but we can add bonus for open squares)
    uint64_t knights = board.getPieceBitboard(color, KNIGHT);
    int knight_count = __builtin_popcountll(knights);
    if (knight_count > 0) {
        // Central knights get bonus (e4, d4, e5, d5, c4, c5, f4, f5)
        uint64_t center = 0x00003C3C3C3C0000ULL;
        int central_knights = __builtin_popcountll(knights & center);
        score += central_knights * weights_.knight_mobility_bonus;
    }

    // Bishop mobility: bishops prefer open diagonals
    uint64_t bishops = board.getPieceBitboard(color, BISHOP);
    uint64_t all_pawns = board.getPieceBitboard(Color::WHITE, PAWN) |
                         board.getPieceBitboard(Color::BLACK, PAWN);
    int bishop_count = __builtin_popcountll(bishops);
    if (bishop_count > 0) {
        // Bishops not blocked by center pawns get bonus
        uint64_t center_pawns = all_pawns & 0x0000001818000000ULL;  // e4/d4/e5/d5
        if (__builtin_popcountll(center_pawns) < 2) {
            // Open center - good for bishops
            score += bishop_count * weights_.bishop_mobility_bonus;
        }
    }

    // Rook mobility: rooks on open/semi-open files
    uint64_t rooks = board.getPieceBitboard(color, ROOK);
    uint64_t friendly_pawns = board.getPieceBitboard(color, PAWN);
    uint64_t enemy_pawns = board.getPieceBitboard(~color, PAWN);

    uint64_t rook_bb = rooks;
    while (rook_bb) {
        Square sq = static_cast<Square>(__builtin_ctzll(rook_bb));
        int file = sq % 8;

        uint64_t file_bb = file_mask(file);
        bool friendly_pawn_on_file = (friendly_pawns & file_bb) != 0;
        bool enemy_pawn_on_file = (enemy_pawns & file_bb) != 0;

        if (!friendly_pawn_on_file && !enemy_pawn_on_file) {
            // Open file - excellent for rook
            score += weights_.rook_open_file;
        } else if (!friendly_pawn_on_file && enemy_pawn_on_file) {
            // Semi-open file - still good
            score += weights_.rook_open_file / 2;
        }

        rook_bb &= rook_bb - 1;
    }

    // Queen mobility: active queen gets bonus
    uint64_t queen = board.getPieceBitboard(color, QUEEN);
    if (queen) {
        Square queen_sq = static_cast<Square>(__builtin_ctzll(queen));
        int queen_rank = queen_sq / 8;

        // Queen on 4th rank or beyond gets activity bonus
        bool queen_active = (color == Color::WHITE) ? (queen_rank >= 3) : (queen_rank <= 4);
        if (queen_active) {
            score += weights_.queen_mobility_bonus * 10;  // Small bonus for active queen
        }
    }

    return score;
}

// Development Evaluation

int HandcraftedEvaluator::evaluate_development(const Board& board, Color color, int phase) const {
    int score = 0;

    // Development only matters in opening phase
    if (phase < 128) {
        // Middlegame/endgame: development doesn't matter
        return 0;
    }

    // Check minor piece development (off back rank)
    int back_rank = (color == Color::WHITE) ? 0 : 7;
    uint64_t back_rank_mask = 0xFFULL << (back_rank * 8);

    uint64_t knights = board.getPieceBitboard(color, KNIGHT);
    uint64_t bishops = board.getPieceBitboard(color, BISHOP);

    // Count developed minor pieces (not on back rank)
    int developed_knights = __builtin_popcountll(knights & ~back_rank_mask);
    int developed_bishops = __builtin_popcountll(bishops & ~back_rank_mask);

    score += (developed_knights + developed_bishops) * weights_.minor_piece_development;

    // Early queen development penalty
    uint64_t queen = board.getPieceBitboard(color, QUEEN);
    if (queen && (queen & back_rank_mask) == 0) {
        // Queen is off back rank
        // Check if minor pieces are still undeveloped
        int undeveloped_minors = __builtin_popcountll((knights | bishops) & back_rank_mask);
        if (undeveloped_minors >= 3) {
            // Queen out too early with most minors still on back rank - penalty
            score -= weights_.early_queen_penalty;
        }
    }

    // Rook development (connected rooks, open files handled in mobility)
    // Rooks on back rank connected if no pieces between them
    uint64_t rooks = board.getPieceBitboard(color, ROOK);
    if (__builtin_popcountll(rooks) == 2) {
        uint64_t back_rank_rooks = rooks & back_rank_mask;
        if (__builtin_popcountll(back_rank_rooks) == 2) {
            // Both rooks on back rank - check if connected
            // Need to check for ALL pieces (friendly + enemy) between rooks
            uint64_t all_pieces = 0;
            for (int pt = PAWN; pt <= KING; ++pt) {
                all_pieces |= board.getPieceBitboard(color, static_cast<PieceType>(pt));
                all_pieces |= board.getPieceBitboard(~color, static_cast<PieceType>(pt));
            }

            // Find rook squares
            uint64_t temp = back_rank_rooks;
            Square rook1 = static_cast<Square>(__builtin_ctzll(temp));
            temp &= temp - 1;
            Square rook2 = static_cast<Square>(__builtin_ctzll(temp));

            int file1 = rook1 % 8;
            int file2 = rook2 % 8;

            // Check if connected (no pieces between on back rank)
            bool connected = true;
            for (int f = std::min(file1, file2) + 1; f < std::max(file1, file2); ++f) {
                Square between = static_cast<Square>(back_rank * 8 + f);  // Fixed: back_rank is index, not square
                if (all_pieces & (1ULL << between)) {
                    connected = false;
                    break;
                }
            }

            if (connected) {
                score += weights_.minor_piece_development / 2;  // Small bonus
            }
        }
    }

    // Scale development by phase (most important in opening)
    score = (score * phase) / 256;

    return score;
}

// ============================================================================
// Pawn Hash Table Implementation (Task 3.6)
// ============================================================================

void HandcraftedEvaluator::clear_pawn_hash() {
    std::fill(pawn_hash_table_.begin(), pawn_hash_table_.end(), PawnHashEntry{});
    pawn_hash_stats_ = PawnHashStats{};
}

size_t HandcraftedEvaluator::get_pawn_hash_memory_usage() const {
    return pawn_hash_table_.size() * PAWN_HASH_ENTRY_SIZE;
}

uint64_t HandcraftedEvaluator::calculate_pawn_key(const Board& board) const {
    uint64_t key = 0ULL;

    // XOR in white pawns (WHITE_PAWN = 0)
    uint64_t white_pawns = board.getPieceBitboard(Color::WHITE, PAWN);
    while (white_pawns) {
        Square sq = static_cast<Square>(__builtin_ctzll(white_pawns));
        key ^= board.zobristPieces[sq][WHITE_PAWN];
        white_pawns &= white_pawns - 1;
    }

    // XOR in black pawns (BLACK_PAWN = 6)
    uint64_t black_pawns = board.getPieceBitboard(Color::BLACK, PAWN);
    while (black_pawns) {
        Square sq = static_cast<Square>(__builtin_ctzll(black_pawns));
        key ^= board.zobristPieces[sq][BLACK_PAWN];
        black_pawns &= black_pawns - 1;
    }

    return key;
}

bool HandcraftedEvaluator::probe_pawn_hash(uint64_t key, PawnHashEntry& entry) const {
    size_t index = key % pawn_hash_table_.size();
    const PawnHashEntry& stored = pawn_hash_table_[index];

    if (stored.key == key) {
        pawn_hash_stats_.hits++;
        entry = stored;
        return true;
    }

    if (stored.key != 0) {
        pawn_hash_stats_.collisions++;
    }

    pawn_hash_stats_.misses++;
    return false;
}

void HandcraftedEvaluator::store_pawn_hash(uint64_t key, int score_mg, int score_eg,
                                          uint8_t white_passers, uint8_t black_passers,
                                          uint16_t flags) {
    size_t index = key % pawn_hash_table_.size();

    pawn_hash_table_[index] = PawnHashEntry{
        key,
        static_cast<int16_t>(score_mg),
        static_cast<int16_t>(score_eg),
        white_passers,
        black_passers,
        flags
    };
}

} // namespace eval
} // namespace opera
