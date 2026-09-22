#include "search/alphabeta.h"
#include <algorithm>
#include <iostream>

namespace opera {

AlphaBetaSearch::AlphaBetaSearch(Board& board, std::atomic<bool>& stop_flag,
                               TranspositionTable& tt, MoveOrdering& move_ordering,
                               StaticExchangeEvaluator& see,
                               eval::Evaluator* evaluator)
    : board(board), stop_flag(stop_flag), tt(tt), move_ordering(move_ordering), see(see), evaluator(evaluator) {
    
    // Initialize PV table
    pv_table.resize(MAX_PLY);
    for (int i = 0; i < MAX_PLY; ++i) {
        pv_table[i].reserve(MAX_PLY - i);
    }
}

void AlphaBetaSearch::set_evaluator(eval::Evaluator* eval) {
    evaluator = eval;
}

int AlphaBetaSearch::search(int depth, int alpha, int beta) {
    // Reset search state
    stats.reset();
    pv_line.clear();
    node_check_counter = 0;
    search_start_time = std::chrono::high_resolution_clock::now();
    
    // Clear PV table
    for (auto& line : pv_table) {
        line.clear();
    }
    
    // Start principal variation search from root
    int score = pvs(depth, 0, alpha, beta, true);
    
    // Extract principal variation
    extract_pv(0);
    
    return score;
}

namespace {
int score_to_tt(int score, int ply) {
    if (score >= CHECKMATE_SCORE - MAX_PLY) return score + ply;
    if (score <= -CHECKMATE_SCORE + MAX_PLY) return score - ply;
    return score;
}
int score_from_tt(int score, int ply) {
    if (score >= CHECKMATE_SCORE - MAX_PLY) return score - ply;
    if (score <= -CHECKMATE_SCORE + MAX_PLY) return score + ply;
    return score;
}
}

int AlphaBetaSearch::pvs(int depth, int ply, int alpha, int beta, bool is_pv_node, bool allow_null) {
    if (depth <= 0) return quiescence(ply, alpha, beta);
    ++stats.nodes;
    if (should_stop()) return alpha;
    if (ply < MAX_PLY) pv_table[ply].clear();
    const bool checked = in_check();
    if (board.isThreefoldRepetition() || board.isFiftyMoveRule() || board.isInsufficientMaterial()) {
        if (checked && board.isCheckmate(board.getSideToMove())) return -CHECKMATE_SCORE + ply;
        return 0;
    }
    if (ply >= MAX_PLY) return checked ? 0 : evaluate();

    const int original_alpha = alpha;
    TTEntry entry;
    const bool hit = tt.probe(board.getZobristKey(), entry);
    // Close to a fifty-move claim, scores from a different reversible history
    // are unsafe. The hash move is still useful for ordering.
    if (hit && entry.get_depth() >= depth && !is_pv_node && board.getHalfmoveClock() < 90 &&
        entry.get_rule50() == board.getHalfmoveClock()) {
        ++stats.tt_hits;
        const int score = score_from_tt(entry.get_score(), ply);
        if (entry.get_type() == TTEntryType::EXACT ||
            (entry.get_type() == TTEntryType::LOWER_BOUND && score >= beta) ||
            (entry.get_type() == TTEntryType::UPPER_BOUND && score <= alpha)) {
            ++stats.tt_cutoffs;
            return score;
        }
    }

    MoveGenList<> moves;
    generateAllMoves(board, moves, board.getSideToMove());
    const int static_eval = checked ? -INFINITY_SCORE : evaluate();
    if (!is_pv_node && !checked && allow_null && depth >= min_depth_for_nmp &&
        static_eval >= beta && std::abs(beta) < CHECKMATE_SCORE - MAX_PLY &&
        board.hasNonPawnMaterial(board.getSideToMove()) && board.getPhase() > 6 &&
        board.getHalfmoveClock() < 90 && !board.isStalemate(board.getSideToMove())) {
        board.makeNullMove();
        const int reduced_depth = std::max(0, depth - 1 - std::max(1, null_move_reduction));
        int score = -pvs(reduced_depth, ply + 1, -beta, -beta + 1, false, false);
        board.unmakeNullMove();
        if (stop_flag.load(std::memory_order_relaxed)) return alpha;
        if (score >= beta && score < CHECKMATE_SCORE - MAX_PLY) {
            // Verify deeper cutoffs without another null move at this node.
            if (depth >= 6) score = pvs(depth - std::max(1, null_move_reduction), ply, beta - 1, beta, false, false);
            if (stop_flag.load(std::memory_order_relaxed)) return alpha;
            if (score >= beta) { ++stats.null_move_cutoffs; return score; }
        }
    }
    if (!is_pv_node && !checked && depth <= min_depth_for_razoring &&
        std::abs(alpha) < CHECKMATE_SCORE - MAX_PLY && can_razor(depth, alpha, static_eval)) {
        const int score = quiescence(ply, alpha - 1, alpha);
        if (score < alpha) { ++stats.razoring_prunes; return score; }
    }
    move_ordering.score_moves(moves, ply);
    move_ordering.sort_moves(moves);
    int best_score = -INFINITY_SCORE;
    Move best_move;
    int legal_moves = 0;
    for (const MoveGen& mg : moves) {
        const Move move = movegen_to_move(mg);
        if (ply == 0 && !root_moves.empty() &&
            std::find(root_moves.begin(), root_moves.end(), move.toString()) == root_moves.end()) continue;
        if (stop_flag.load(std::memory_order_relaxed)) return alpha;
        if (!board.makeGeneratedMove(mg)) continue;
        ++legal_moves;
        const bool gives_check = in_check();
        const int extension = ply < 2 * depth ? get_extensions(mg, checked, gives_check) : 0;
        stats.extensions += extension;
        if (!is_pv_node && depth <= min_depth_for_futility && !checked && !gives_check &&
            !mg.isCapture() && !mg.isPromotion() && legal_moves > 1 &&
            std::abs(alpha) < CHECKMATE_SCORE - MAX_PLY && can_futility_prune(depth, alpha, static_eval)) {
            board.unmakeMove(mg);
            ++stats.futility_prunes;
            continue;
        }
        const int next_depth = depth - 1 + extension;
        int reduction = (!checked && !gives_check && !extension && depth >= min_depth_for_lmr) ?
            std::min(std::max(0, next_depth - 1), get_lmr_reduction(depth, legal_moves - 1, is_pv_node, mg)) : 0;
        if (reduction) { ++stats.lmr_reductions; stats.reductions += reduction; }
        int score;
        if (legal_moves == 1) {
            score = -pvs(next_depth, ply + 1, -beta, -alpha, is_pv_node);
        } else {
            score = -pvs(next_depth - reduction, ply + 1, -alpha - 1, -alpha, false);
            if (reduction && score > alpha)
                score = -pvs(next_depth, ply + 1, -alpha - 1, -alpha, false);
            if (is_pv_node && score > alpha && score < beta)
                score = -pvs(next_depth, ply + 1, -beta, -alpha, true);
        }
        board.unmakeMove(mg);
        if (stop_flag.load(std::memory_order_relaxed)) return alpha;
        if (score > best_score) { best_score = score; best_move = move; }
        if (score > alpha) {
            alpha = score;
            pv_table[ply] = {move};
            if (ply + 1 < MAX_PLY)
                pv_table[ply].insert(pv_table[ply].end(), pv_table[ply + 1].begin(), pv_table[ply + 1].end());
            if (score >= beta) {
                ++stats.beta_cutoffs;
                if (legal_moves == 1) ++stats.first_move_cutoffs;
                if (!mg.isCapture() && !mg.isPromotion()) {
                    move_ordering.store_killer_move(mg, ply);
                    move_ordering.update_history(mg, board.getSideToMove(), depth);
                }
                break;
            }
        }
    }
    if (!legal_moves) return checked ? -CHECKMATE_SCORE + ply : 0;
    const TTEntryType bound = best_score <= original_alpha ? TTEntryType::UPPER_BOUND :
                              best_score >= beta ? TTEntryType::LOWER_BOUND : TTEntryType::EXACT;
    // A restricted root score describes only searchmoves, not the whole position.
    if (!(ply == 0 && !root_moves.empty()) && board.getHalfmoveClock() < 90)
        tt.store(board.getZobristKey(), best_move, score_to_tt(best_score, ply), depth, bound, board.getHalfmoveClock());
    return best_score;
}

int AlphaBetaSearch::quiescence(int ply, int alpha, int beta, int checking_plies) {
    ++stats.nodes;
    if (should_stop()) return alpha;
    if (ply < MAX_PLY) pv_table[ply].clear();
    const bool checked = in_check();
    MoveGenList<> all_moves, tactical;
    generateAllMoves(board, all_moves, board.getSideToMove());
    bool legal = false;
    for (const auto& move : all_moves) {
        if (!legal && board.isLegalGeneratedMove(move, board.getSideToMove())) legal = true;
        if (checked || move.isCapture() || move.isPromotion() || move.isEnPassant()) tactical.add(move);
    }
    if (!legal) return checked ? -CHECKMATE_SCORE + ply : 0;
    if (board.isThreefoldRepetition() || board.isFiftyMoveRule() || board.isInsufficientMaterial()) return 0;
    if (ply >= MAX_PLY) return checked ? 0 : evaluate();
    int best = checked ? -INFINITY_SCORE : evaluate();
    if (best >= beta) return best;
    alpha = std::max(alpha, best);
    // Include quiet checks at the horizon entry, then continue with captures
    // and mandatory evasions. Do not restart this allowance after each capture:
    // that expands long exchange sequences far beyond the intended budget.
    if (!checked && checking_plies > 0) {
        for (const auto& move : all_moves)
            if (!move.isCapture() && !move.isPromotion() && !move.isEnPassant() && board.givesCheck(move))
                tactical.add(move);
    }
    move_ordering.score_moves(tactical, ply);
    move_ordering.sort_moves(tactical);
    for (const auto& move : tactical) {
        const int exchange = checked || move.isPromotion() ? 0 : see.evaluate(move);
        if (!board.makeGeneratedMove(move)) continue;
        // Never discard checking sacrifices or promotions on a material-only SEE.
        if (!checked && exchange < 0 && !in_check()) {
            board.unmakeMove(move);
            continue;
        }
        const int score = -quiescence(ply + 1, -beta, -alpha,
                                     std::max(0, checking_plies - 1));
        board.unmakeMove(move);
        if (stop_flag.load(std::memory_order_relaxed)) return alpha;
        best = std::max(best, score);
        if (score > alpha) {
            alpha = score;
            pv_table[ply] = {movegen_to_move(move)};
            if (ply + 1 < MAX_PLY)
                pv_table[ply].insert(pv_table[ply].end(), pv_table[ply + 1].begin(), pv_table[ply + 1].end());
        }
        if (score >= beta) return score;
    }
    return best;
}

const std::vector<Move>& AlphaBetaSearch::get_principal_variation() const {
    return pv_line;
}

const SearchStats& AlphaBetaSearch::get_stats() const {
    return stats;
}

void AlphaBetaSearch::reset() {
    stats.reset();
    pv_line.clear();
    node_check_counter = 0;
    
    for (auto& line : pv_table) {
        line.clear();
    }
}

void AlphaBetaSearch::clear_history() {
    move_ordering.reset();
}

int AlphaBetaSearch::evaluate() {
    // Use evaluator if available, otherwise fall back to material-only
    if (evaluator) {
        const Color side = board.getSideToMove();
        const int white_score = evaluator->evaluate(board, side);
        return side == WHITE ? white_score : -white_score;
    }

    // Fallback: Basic material evaluation
    int material = 0;

    Color us = board.getSideToMove();
    for (int pieceType = PAWN; pieceType <= QUEEN; ++pieceType) {
        material += __builtin_popcountll(board.getPieceBitboard(us, static_cast<PieceType>(pieceType))) *
                   (pieceType == PAWN ? 100 : pieceType == KNIGHT ? 320 : pieceType == BISHOP ? 330 :
                    pieceType == ROOK ? 500 : 900);
        material -= __builtin_popcountll(board.getPieceBitboard(~us, static_cast<PieceType>(pieceType))) *
                   (pieceType == PAWN ? 100 : pieceType == KNIGHT ? 320 : pieceType == BISHOP ? 330 :
                    pieceType == ROOK ? 500 : 900);
    }

    return material;
}

bool AlphaBetaSearch::in_check() const {
    Color us = board.getSideToMove();
    Square our_king = board.getKingSquare(us);
    return board.isSquareAttacked(our_king, ~us);
}

int AlphaBetaSearch::get_extensions(const MoveGen& move, bool /* in_check */, bool gives_check) {
    int extension = 0;
    
    // Check extension
    if (gives_check) {
        extension += CHECK_EXTENSION;
    }
    
    // Passed pawn extension (simplified)
    // Called after makeMove: the moving piece is now on the destination.
    Piece piece = board.getPiece(move.to());
    if (typeOf(piece) == PAWN) {
        Square to = move.to();
        Rank rank = rankOf(to);
        Color us = ~board.getSideToMove();
        
        if ((us == WHITE && rank >= 6) || (us == BLACK && rank <= 1)) {
            extension += PASSED_PAWN_EXTENSION;
        }
    }
    
    // Limit total extension
    return std::min(extension, 2);
}

bool AlphaBetaSearch::should_stop() {
    if (stop_flag.load(std::memory_order_relaxed)) return true;
    if (stop_check && stop_check()) { stop_flag.store(true, std::memory_order_relaxed); return true; }
    return false;
}

Move AlphaBetaSearch::movegen_to_move(const MoveGen& mg) const {
    return Move(mg.from(), mg.to(), mg.isPromotion() ? PROMOTION :
        mg.isCastling() ? CASTLING : mg.isEnPassant() ? EN_PASSANT : NORMAL,
        mg.isPromotion() ? typeOf(mg.promotionPiece()) : NO_PIECE_TYPE);
}

void AlphaBetaSearch::extract_pv(int ply) {
    pv_line.clear();
    
    if (ply < static_cast<int>(pv_table.size())) {
        pv_line = pv_table[ply];
    }
}

int AlphaBetaSearch::get_lmr_reduction(int depth, int move_number, bool is_pv_node, const MoveGen& move) const {
    // Don't reduce:
    // - PV nodes
    // - First few moves
    // - Tactical moves (captures, promotions, checks)
    if (is_pv_node || move_number < lmr_full_depth_moves) {
        return 0;
    }
    
    if (move.isCapture() || move.isPromotion()) {
        return 0;
    }
    
    // Calculate reduction based on depth and move number
    int reduction = 1;  // Base reduction
    
    if (depth >= 6 && move_number >= 8) {
        reduction = 2;
    }
    
    if (depth >= 8 && move_number >= 12) {
        reduction = 3;
    }
    
    return std::min(reduction, lmr_reduction_limit);
}

bool AlphaBetaSearch::can_futility_prune(int depth, int alpha, int static_eval) const {
    // Futility pruning: if static eval + margin is still below alpha,
    // remaining moves are unlikely to raise alpha
    return static_eval + futility_margin * depth < alpha;
}

bool AlphaBetaSearch::can_razor(int depth, int alpha, int static_eval) const {
    // Razoring: if static eval + margin is below alpha,
    // do a qsearch to verify the position is really bad
    return static_eval + razoring_margin < alpha;
}

} // namespace opera
