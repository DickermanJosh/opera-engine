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
    EvaluationTerms terms;
    return evaluate_with_terms(board, side_to_move, terms);
}

int HandcraftedEvaluator::evaluate_with_terms(const Board& board, Color side_to_move, EvaluationTerms& terms, bool collect_activity) {
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

    // Reuse the same sliding rays for mobility, pressure and usable defenders.
    const AttackInfo attacks = analyze_attacks(board);
    int white_king_safety = evaluate_king_safety(board, Color::WHITE, phase, attacks);
    int black_king_safety = evaluate_king_safety(board, Color::BLACK, phase, attacks);

    MobilityDetails activity[2];
    collect_activity = collect_activity && phase > 128;
    int white_mobility = evaluate_mobility(board, Color::WHITE, collect_activity ? &activity[WHITE] : nullptr, attacks);
    int black_mobility = evaluate_mobility(board, Color::BLACK, collect_activity ? &activity[BLACK] : nullptr, attacks);

    int white_development = evaluate_development(board, Color::WHITE, phase);
    int black_development = evaluate_development(board, Color::BLACK, phase);
    const int white_threats = evaluate_threats(board, WHITE, attacks);
    const int black_threats = evaluate_threats(board, BLACK, attacks);

    terms = {phase, {white_material, black_material}, {white_king_safety, black_king_safety},
             {white_mobility, black_mobility}, {white_development, black_development},
             {white_threats, black_threats}, {activity[WHITE], activity[BLACK]}};
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
        development_score * weights_.development_weight + white_threats - black_threats
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

// Attack maps distinguish usable moves/defenders from geometric king attacks.
HandcraftedEvaluator::AttackInfo HandcraftedEvaluator::analyze_attacks(const Board& board) const {
    AttackInfo result;
    Bitboard pin_lines[64]; // Read only for squares set in pinned[].
    const Bitboard occupied = board.getOccupiedBitboard();
    for (Color color : {WHITE, BLACK}) {
        const Square king = board.getKingSquare(color);
        if (king == NO_SQUARE) continue;
        const Bitboard own = board.getColorBitboard(color);
        const Bitboard queens = board.getPieceBitboard(~color, QUEEN);
        Bitboard pinners = (board.getBishopAttacks(king, occupied & ~own) &
                            (queens | board.getPieceBitboard(~color, BISHOP))) |
                           (board.getRookAttacks(king, occupied & ~own) &
                            (queens | board.getPieceBitboard(~color, ROOK)));
        while (pinners) {
            const Square pinner = static_cast<Square>(__builtin_ctzll(pinners));
            pinners &= pinners - 1;
            const int df = (fileOf(pinner) > fileOf(king)) - (fileOf(pinner) < fileOf(king));
            const int dr = (rankOf(pinner) > rankOf(king)) - (rankOf(pinner) < rankOf(king));
            const int step = dr * 8 + df;
            Bitboard between = 0;
            for (int sq = king + step; sq != pinner; sq += step) between |= 1ULL << sq;
            const Bitboard blockers = between & occupied;
            if (blockers && !(blockers & (blockers - 1)) && (blockers & own)) {
                result.pinned[color] |= blockers;
                pin_lines[__builtin_ctzll(blockers)] = between | (1ULL << pinner);
            }
        }
    }
    for (Color color : {WHITE, BLACK}) {
        const Bitboard enemy_king = board.getPieceBitboard(~color, KING);
        for (int pt = PAWN; pt <= KING; ++pt) {
            Bitboard pieces = board.getPieceBitboard(color, static_cast<PieceType>(pt));
            while (pieces) {
                const Square sq = static_cast<Square>(__builtin_ctzll(pieces));
                const Bitboard bit = 1ULL << sq;
                pieces &= pieces - 1;
                Bitboard raw = pt == PAWN ? board.getPawnAttacks(sq, color) :
                    pt == KNIGHT ? board.getKnightAttacks(sq) :
                    pt == BISHOP ? board.getBishopAttacks(sq, occupied) :
                    pt == ROOK ? board.getRookAttacks(sq, occupied) :
                    pt == QUEEN ? board.getQueenAttacks(sq, occupied) : board.getKingAttacks(sq);
                // A king cannot escape along a checking ray by hiding behind
                // the square it just vacated. This map also retains pinned attacks.
                Bitboard danger = raw;
                if ((raw & enemy_king) && pt >= BISHOP && pt <= QUEEN) {
                    const Bitboard without_king = occupied & ~enemy_king;
                    danger = pt == BISHOP ? board.getBishopAttacks(sq, without_king) :
                             pt == ROOK ? board.getRookAttacks(sq, without_king) :
                                          board.getQueenAttacks(sq, without_king);
                }
                result.king_danger[color] |= danger;
                if (result.pinned[color] & bit) raw &= pin_lines[sq];
                result.piece[sq] = raw;
                result.twice[color] |= result.all[color] & raw;
                result.all[color] |= raw;
                result.by_type[color][pt] |= raw;
            }
        }
    }
    return result;
}

// King Safety Evaluation

int HandcraftedEvaluator::evaluate_king_safety(const Board& board, Color color, int phase) const {
    return evaluate_king_safety(board, color, phase, analyze_attacks(board));
}

int HandcraftedEvaluator::evaluate_king_safety(const Board& board, Color color, int phase, const AttackInfo& attacks) const {
    if (!phase) return 0;
    const Square king = board.getKingSquare(color);
    if (king == NO_SQUARE) return 0;
    const int file = fileOf(king), rank = rankOf(king);
    const int relative_rank = color == WHITE ? rank : 7 - rank;
    const Bitboard pawns = board.getPieceBitboard(color, PAWN);
    int score = -relative_rank * 16;
    for (int f = std::max(0, file - 1); f <= std::min(7, file + 1); ++f) {
        Bitboard file_pawns = pawns & file_mask(f);
        int distance = 8;
        while (file_pawns) {
            const Square pawn = static_cast<Square>(__builtin_ctzll(file_pawns));
            file_pawns &= file_pawns - 1;
            const int ahead = color == WHITE ? rankOf(pawn) - rank : rank - rankOf(pawn);
            if (ahead > 0) distance = std::min(distance, ahead);
        }
        if (distance == 1) score += weights_.pawn_shield_bonus;
        else if (distance == 2) score += weights_.pawn_shield_bonus / 2;
        else score -= weights_.open_file_near_king_penalty;
    }
    // Keep the option to castle; walking out of the centre must not erase its cost.
    if (file >= 3 && file <= 5) {
        const bool rights = board.canCastleKingside(color) || board.canCastleQueenside(color);
        score -= rights ? 12 : 32;
    }

    // Include the next rank in front of the king: a rook lift or an invading
    // rook can matter before it attacks the king's immediately adjacent squares.
    const Color enemy = ~color;
    const Bitboard adjacent = board.getKingAttacks(king);
    const Bitboard zone = adjacent | (1ULL << king) |
        (color == WHITE ? adjacent << 8 : adjacent >> 8);
    const Bitboard occupancy = board.getOccupiedBitboard();
    const Bitboard own = board.getColorBitboard(color);
    const Bitboard non_king_defense = attacks.by_type[color][PAWN] | attacks.by_type[color][KNIGHT] |
        attacks.by_type[color][BISHOP] | attacks.by_type[color][ROOK] | attacks.by_type[color][QUEEN];
    const Bitboard check_safe = ~attacks.all[color] |
        (attacks.twice[enemy] & attacks.by_type[color][KING] & ~non_king_defense);
    const Bitboard check_targets = check_safe & ~board.getColorBitboard(enemy);
    const Bitboard diagonal_checks = board.getBishopAttacks(king, occupancy);
    const Bitboard straight_checks = board.getRookAttacks(king, occupancy);
    const Bitboard knight_checks = board.getKnightAttacks(king);
    const int pressure_weight[] = {0, 4, 3, 4, 6};
    const int check_weight[] = {0, 12, 8, 14, 10};
    int attackers = 0, pressure = 0, checking_access = 0;
    for (int pt = KNIGHT; pt <= QUEEN; ++pt) {
        Bitboard pieces = board.getPieceBitboard(enemy, static_cast<PieceType>(pt));
        Bitboard checks_for_type = 0;
        while (pieces) {
            const Square sq = static_cast<Square>(__builtin_ctzll(pieces));
            pieces &= pieces - 1;
            const Bitboard usable = attacks.piece[sq];
            const int hits = __builtin_popcountll(usable & zone);
            const Bitboard checks = usable & check_targets & (pt == KNIGHT ? knight_checks :
                pt == BISHOP ? diagonal_checks : pt == ROOK ? straight_checks : diagonal_checks | straight_checks);
            if (hits || checks) ++attackers;
            pressure += std::min(4, hits) * pressure_weight[pt];
            checks_for_type |= checks;
        }
        checking_access += std::min(2, __builtin_popcountll(checks_for_type)) * check_weight[pt];
    }
    int danger = (pressure + checking_access) * phase / 256;
    if (attackers >= 2) {
        const Bitboard useful_defense = attacks.by_type[color][PAWN] | attacks.by_type[color][KNIGHT] |
            attacks.by_type[color][BISHOP] | attacks.by_type[color][ROOK];
        const int weak = __builtin_popcountll(zone & attacks.all[enemy] & ~attacks.twice[color] & ~useful_defense);
        const int flights = __builtin_popcountll(adjacent & ~own & ~attacks.king_danger[enemy]);
        const int units = pressure + checking_access + 3 * attackers + 4 * weak + 6 * std::max(0, 2 - flights);
        const Bitboard denied = adjacent & ~own & attacks.king_danger[enemy];
        // Potential checking squares alone do not establish a mating net.
        // Preserve nonlinear danger in thin positions only when the attackers
        // actually constrain escape; otherwise king activity must remain viable.
        if (flights <= 1 && denied) danger += std::min(400, units * units / 32);
        // Enemy control of every available escape is a more concrete mating
        // constraint than a king temporarily boxed in only by its own pieces.
        if (flights == 0 && denied) danger += 24;
    }
    // Shelter and potential pressure fade with phase. A constrained mating net
    // must not disappear simply because unrelated pieces have been exchanged.
    return score * phase / 256 - danger;
}

int HandcraftedEvaluator::evaluate_mobility(const Board& board, Color color, MobilityDetails* details) const {
    return evaluate_mobility(board, color, details, analyze_attacks(board));
}

int HandcraftedEvaluator::evaluate_mobility(const Board& board, Color color, MobilityDetails* details, const AttackInfo& attacks) const {
    const Bitboard own = board.getColorBitboard(color);
    const Bitboard our_pawns = board.getPieceBitboard(color, PAWN);
    const Bitboard enemy_pawns = board.getPieceBitboard(~color, PAWN);
    const Bitboard pawn_attacks = attacks.by_type[~color][PAWN];
    const Bitboard safe = ~own & ~pawn_attacks;
    const int weights[] = {0, weights_.knight_mobility_bonus, weights_.bishop_mobility_bonus,
                           weights_.rook_mobility_bonus, weights_.queen_mobility_bonus};
    int score = __builtin_popcountll(board.getPieceBitboard(color, BISHOP)) >= 2 ? 25 : 0;
    for (int pt = KNIGHT; pt <= QUEEN; ++pt) {
        Bitboard pieces = board.getPieceBitboard(color, static_cast<PieceType>(pt));
        while (pieces) {
            const Square sq = static_cast<Square>(__builtin_ctzll(pieces));
            pieces &= pieces - 1;
            const Bitboard usable = attacks.piece[sq];
            score += __builtin_popcountll(usable & safe) * weights[pt];
            // Collect opening activity from the same attack lookup. Morphy's
            // richer evaluation need not generate these sliding rays twice.
            if (details) {
                const Bitboard home = color == WHITE ? 0xffULL : 0xff00000000000000ULL;
                if (pt == KNIGHT || pt == BISHOP) {
                    const Bitboard useful = usable & safe & ~home;
                    const int count = __builtin_popcountll(useful);
                    if ((1ULL << sq) & home) {
                        if (pt == BISHOP) {
                            details->bishop_exits += std::min(3, count);
                            details->blocked_bishops += count == 0;
                        }
                    } else {
                        details->minor_safe_squares += std::min(4, count);
                        details->minor_centre_control += __builtin_popcountll(useful & 0x0000001818000000ULL);
                    }
                } else if (pt == ROOK && (usable & pieces)) {
                    ++details->connected_rook_pairs;
                }
            }
            if (pt == ROOK) {
                if (!(our_pawns & file_mask(fileOf(sq)))) {
                    score += (enemy_pawns & file_mask(fileOf(sq))) ? weights_.rook_open_file / 2 : weights_.rook_open_file;
                    if (usable & board.getPieceBitboard(~color, ROOK)) score += 8;
                }
                const Square enemy_king = board.getKingSquare(~color);
                const int relative_rank = color == WHITE ? rankOf(sq) : 7 - rankOf(sq);
                const int king_rank = enemy_king == NO_SQUARE ? 0 :
                    (color == WHITE ? rankOf(enemy_king) : 7 - rankOf(enemy_king));
                if (relative_rank == 6 && king_rank >= 6 && !(pawn_attacks & (1ULL << sq))) score += 16;
            }
        }
    }
    return score;
}

int HandcraftedEvaluator::evaluate_threats(const Board& board, Color color, const AttackInfo& attacks) const {
    // Threats can gain a useful tempo without giving check. Pay once per target,
    // not once per attacking piece; these modest terms are not material wins.
    const Color enemy = ~color;
    // Attacking a pawn can invite it to advance and chase the attacking piece.
    // Search accounts for winning pawns; this term rewards pressure on pieces.
    Bitboard targets = board.getColorBitboard(enemy) &
        ~(board.getPieceBitboard(enemy, KING) | board.getPieceBitboard(enemy, PAWN)) & attacks.all[color];
    int score = 0;
    while (targets) {
        const Square sq = static_cast<Square>(__builtin_ctzll(targets));
        const Bitboard bit = 1ULL << sq;
        targets &= targets - 1;
        const PieceType type = typeOf(board.getPiece(sq));
        int threat = 0;
        if (!(attacks.all[enemy] & bit)) threat = type == QUEEN ? 35 : 20;
        if (attacks.by_type[color][PAWN] & bit) threat = std::max(threat, 30);
        if (type >= ROOK && ((attacks.by_type[color][KNIGHT] | attacks.by_type[color][BISHOP]) & bit))
            threat = std::max(threat, type == QUEEN ? 30 : 20);
        if (type == QUEEN && (attacks.by_type[color][ROOK] & bit)) threat = std::max(threat, 25);
        score += threat;
    }
    return std::min(120, score);
}

// Development Evaluation

int HandcraftedEvaluator::evaluate_development(const Board& board, Color color, int phase) const {
    int score = 0;

    // Fade continuously as pieces are exchanged.
    if (!phase) return 0;

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
