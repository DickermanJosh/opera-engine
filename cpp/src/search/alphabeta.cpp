#include "search/alphabeta.h"
#include <algorithm>
#include <iostream>

namespace opera {

AlphaBetaSearch::AlphaBetaSearch(Board& board, std::atomic<bool>& stop_flag,
                               TranspositionTable& tt, MoveOrdering& move_ordering,
                               StaticExchangeEvaluator& see,
                               eval::Evaluator* evaluator)
    : board(board), stop_flag(stop_flag), tt(tt), move_ordering(move_ordering), see(see), evaluator(evaluator) {
    
    // Initialize killer moves
    for (int i = 0; i < MAX_PLY; ++i) {
        killer_moves[i][0] = Move();
        killer_moves[i][1] = Move();
    }
    
    // Initialize history table
    for (int from = 0; from < 64; ++from) {
        for (int to = 0; to < 64; ++to) {
            history_table[from][to] = 0;
        }
    }
    
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

int AlphaBetaSearch::pvs(int depth, int ply, int alpha, int beta, bool is_pv_node) {
    stats.nodes++;
    
    // Check for search termination frequently for responsive control
    if ((node_check_counter++ & 255) == 0 && should_stop()) {
        return alpha;
    }
    
    // Terminal node - call quiescence
    if (depth <= 0) {
        return quiescence(ply, alpha, beta);
    }
    
    // Check for maximum ply reached
    if (ply >= MAX_PLY) {
        return evaluate();
    }
    
    bool in_check_flag = in_check();
    int original_alpha = alpha;
    
    // Transposition table lookup
    TTEntry tt_entry;
    bool tt_hit = tt.probe(board.getZobristKey(), tt_entry);
    
    if (tt_hit && tt_entry.get_depth() >= depth && !is_pv_node) {
        stats.tt_hits++;
        
        if (tt_entry.get_type() == TTEntryType::EXACT) {
            stats.tt_cutoffs++;
            return tt_entry.get_score();
        } else if (tt_entry.get_type() == TTEntryType::LOWER_BOUND && tt_entry.get_score() >= beta) {
            stats.tt_cutoffs++;
            return beta;
        } else if (tt_entry.get_type() == TTEntryType::UPPER_BOUND && tt_entry.get_score() <= alpha) {
            stats.tt_cutoffs++;
            return alpha;
        }
    }
    
    // Generate and order moves
    MoveGenList<256> moves;
    generateAllMoves(board, moves, board.getSideToMove());
    
    // Check for checkmate/stalemate
    if (moves.size() == 0) {
        if (in_check_flag) {
            return -CHECKMATE_SCORE + ply;  // Checkmate - prefer shorter mates
        } else {
            return 0;  // Stalemate
        }
    }
    
    // Get static evaluation for pruning decisions
    int static_eval = evaluate();
    
    // Null Move Pruning (before move generation to save time)
    if (depth >= min_depth_for_nmp && !is_pv_node && 
        can_do_null_move(in_check_flag) && static_eval >= beta) {
        
        // Make null move
        make_null_move();
        
        // Search with reduced depth
        int null_score = -pvs(depth - 1 - null_move_reduction, ply + 1, -beta, -beta + 1, false);
        
        // Unmake null move  
        unmake_null_move();
        
        if (null_score >= beta) {
            stats.null_move_cutoffs++;
            return beta;  // Null move cutoff
        }
    }
    
    // Razoring - if position looks very bad, verify with qsearch
    if (depth >= min_depth_for_razoring && !is_pv_node && !in_check_flag &&
        can_razor(depth, alpha, static_eval)) {
        
        int razor_score = quiescence(ply, alpha, beta);
        if (razor_score < alpha) {
            stats.razoring_prunes++;
            return razor_score;
        }
    }
    
    // Score and sort moves using move ordering system
    move_ordering.score_moves(moves, ply);
    move_ordering.sort_moves(moves);
    
    int best_score = -INFINITY_SCORE;
    Move best_move;
    bool found_pv = false;
    int legal_moves = 0;
    
    // Search all moves
    for (size_t i = 0; i < moves.size(); ++i) {
        const MoveGen& move_gen = moves[i];
        Move move = movegen_to_move(move_gen);
        
        if (should_stop()) {
            break;
        }
        
        // Make move
        if (!board.makeMove(move_gen)) {
            continue;  // Illegal move
        }
        
        legal_moves++;
        bool gives_check = in_check();
        
        // Calculate extensions
        int extension = get_extensions(move_gen, in_check_flag, gives_check);
        stats.extensions += extension;
        
        // Futility Pruning - skip quiet moves that can't improve alpha
        if (depth <= min_depth_for_futility && !in_check_flag && !gives_check &&
            !move_gen.isCapture() && !move_gen.isPromotion() &&
            legal_moves > 1 && can_futility_prune(depth, alpha, static_eval)) {
            
            board.unmakeMove(move_gen);  // Unmake before continuing
            stats.futility_prunes++;
            continue;  // Skip this move
        }
        
        int score;
        int reduction = 0;
        
        // Late Move Reductions - reduce depth for later moves
        if (depth >= min_depth_for_lmr && !in_check_flag && !gives_check && extension == 0) {
            reduction = get_lmr_reduction(depth, legal_moves - 1, is_pv_node, move_gen);
            if (reduction > 0) {
                stats.lmr_reductions++;
                stats.reductions += reduction;
            }
        }
        
        if (legal_moves == 1 || !is_pv_node) {
            // First move or non-PV node - full window search
            score = -pvs(depth - 1 + extension - reduction, ply + 1, -beta, -alpha, is_pv_node && legal_moves == 1);
        } else {
            // PVS: Search with null window first
            score = -pvs(depth - 1 + extension - reduction, ply + 1, -alpha - 1, -alpha, false);
            
            // If it beats alpha, re-search with full window
            if (score > alpha && score < beta) {
                score = -pvs(depth - 1 + extension, ply + 1, -beta, -alpha, true);  // Full depth re-search
            }
        }
        
        // If LMR failed high, re-search at full depth
        if (reduction > 0 && score > alpha) {
            score = -pvs(depth - 1 + extension, ply + 1, -beta, -alpha, is_pv_node);
        }
        
        // Unmake move
        board.unmakeMove(move_gen);
        
        if (score > best_score) {
            best_score = score;
            best_move = move;
            
            if (score > alpha) {
                alpha = score;
                found_pv = true;
                
                // Update PV
                pv_table[ply].clear();
                pv_table[ply].push_back(move);
                if (ply + 1 < MAX_PLY) {
                    pv_table[ply].insert(pv_table[ply].end(), 
                                       pv_table[ply + 1].begin(), 
                                       pv_table[ply + 1].end());
                }
                
                if (score >= beta) {
                    // Beta cutoff
                    stats.beta_cutoffs++;
                    if (legal_moves == 1) {
                        stats.first_move_cutoffs++;
                    }
                    
                    // Update killer moves and history for non-captures
                    if (!move_gen.isCapture() && !move_gen.isPromotion()) {
                        update_killers(move, ply);
                        update_history(move, depth);
                    }
                    
                    break;
                }
            }
        }
    }
    
    // Handle case where no legal moves were found
    if (legal_moves == 0) {
        if (in_check_flag) {
            return -CHECKMATE_SCORE + ply;
        } else {
            return 0;  // Stalemate
        }
    }
    
    // Store in transposition table
    TTEntryType tt_type;
    if (best_score <= original_alpha) {
        tt_type = TTEntryType::UPPER_BOUND;
    } else if (best_score >= beta) {
        tt_type = TTEntryType::LOWER_BOUND;
    } else {
        tt_type = TTEntryType::EXACT;
    }
    
    tt.store(board.getZobristKey(), best_move, best_score, depth, tt_type);
    
    return best_score;
}

int AlphaBetaSearch::quiescence(int ply, int alpha, int beta) {
    stats.nodes++;
    
    // Check for search termination
    if ((node_check_counter++ & 1023) == 0 && should_stop()) {
        return alpha;
    }
    
    // Maximum ply reached
    if (ply >= MAX_PLY) {
        return evaluate();
    }
    
    // Stand pat evaluation
    int stand_pat = evaluate();
    
    if (stand_pat >= beta) {
        return beta;
    }
    
    if (stand_pat > alpha) {
        alpha = stand_pat;
    }
    
    // Generate captures and checks
    MoveGenList<256> captures;
    generateCaptureMoves(board, captures, board.getSideToMove());
    
    // Score and sort captures by SEE and MVV-LVA
    move_ordering.score_moves(captures, ply);
    move_ordering.sort_moves(captures);
    
    // Search captures
    for (size_t i = 0; i < captures.size(); ++i) {
        const MoveGen& capture = captures[i];
        
        if (should_stop()) {
            break;
        }
        
        // SEE pruning - skip losing captures
        if (see.evaluate(capture) < 0) {
            continue;
        }
        
        // Make move
        if (!board.makeMove(capture)) {
            continue;
        }
        
        int score = -quiescence(ply + 1, -beta, -alpha);
        
        // Unmake move
        board.unmakeMove(capture);
        
        if (score >= beta) {
            return beta;
        }
        
        if (score > alpha) {
            alpha = score;
        }
    }
    
    return alpha;
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
    // Clear killer moves
    for (int i = 0; i < MAX_PLY; ++i) {
        killer_moves[i][0] = Move();
        killer_moves[i][1] = Move();
    }
    
    // Clear history table
    for (int from = 0; from < 64; ++from) {
        for (int to = 0; to < 64; ++to) {
            history_table[from][to] = 0;
        }
    }
}

int AlphaBetaSearch::evaluate() {
    // Use evaluator if available, otherwise fall back to material-only
    if (evaluator) {
        return evaluator->evaluate(board, board.getSideToMove());
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

int AlphaBetaSearch::get_extensions(const MoveGen& move, bool in_check, bool gives_check) {
    int extension = 0;
    
    // Check extension
    if (gives_check) {
        extension += CHECK_EXTENSION;
    }
    
    // Passed pawn extension (simplified)
    Piece piece = board.getPiece(move.from());
    if (typeOf(piece) == PAWN) {
        Square to = move.to();
        Rank rank = rankOf(to);
        Color us = board.getSideToMove();
        
        if ((us == WHITE && rank >= 6) || (us == BLACK && rank <= 1)) {
            extension += PASSED_PAWN_EXTENSION;
        }
    }
    
    // Limit total extension
    return std::min(extension, 2);
}

void AlphaBetaSearch::update_killers(const Move& move, int ply) {
    if (ply >= MAX_PLY) return;
    
    // Don't duplicate killers
    if (killer_moves[ply][0] == move) {
        return;
    }
    
    // Shift killers
    killer_moves[ply][1] = killer_moves[ply][0];
    killer_moves[ply][0] = move;
}

void AlphaBetaSearch::update_history(const Move& move, int depth) {
    int from = static_cast<int>(move.from());
    int to = static_cast<int>(move.to());
    
    if (from >= 0 && from < 64 && to >= 0 && to < 64) {
        history_table[from][to] += depth * depth;
        
        // Prevent overflow
        if (history_table[from][to] > 1000000) {
            // Age history table
            for (int f = 0; f < 64; ++f) {
                for (int t = 0; t < 64; ++t) {
                    history_table[f][t] /= 2;
                }
            }
        }
    }
}

bool AlphaBetaSearch::should_stop() {
    return stop_flag.load();
}

MoveGen AlphaBetaSearch::move_to_movegen(const Move& move) const {
    // Convert internal Move to MoveGen
    Square from = move.from();
    Square to = move.to();
    
    // Get piece at from square
    Piece piece = board.getPiece(from);
    
    return MoveGen(from, to);
}

Move AlphaBetaSearch::movegen_to_move(const MoveGen& mg) const {
    return Move(mg.from(), mg.to());
}

void AlphaBetaSearch::extract_pv(int ply) {
    pv_line.clear();
    
    if (ply < static_cast<int>(pv_table.size())) {
        pv_line = pv_table[ply];
    }
}

bool AlphaBetaSearch::can_do_null_move(bool in_check) const {
    // TODO: Implement proper null move pruning
    // For now, disabled until we have proper board state management
    return false;
    
    // Future implementation:
    /*
    if (in_check) {
        return false;
    }
    
    Color us = board.getSideToMove();
    
    // Count non-pawn, non-king pieces
    int non_pawn_pieces = 0;
    for (PieceType pt = KNIGHT; pt <= QUEEN; ++pt) {
        non_pawn_pieces += popcount(board.getPieceBitboard(us, pt));
    }
    
    // Need at least one non-pawn piece to safely do null move
    return non_pawn_pieces > 0;
    */
}

void AlphaBetaSearch::make_null_move() {
    // Simple null move - just switch sides
    // This is a simplified implementation
    // In practice, we need to handle en passant and other state
    
    // For now, disable null move pruning by making it a no-op
    // TODO: Implement proper null move with board state management
}

void AlphaBetaSearch::unmake_null_move() {
    // Companion to make_null_move()
    // For now, this is a no-op matching make_null_move()
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

bool AlphaBetaSearch::should_stop() const {
    // Check external stop flag
    if (stop_flag.load()) {
        return true;
    }
    
    // Basic time check (could add more sophisticated time management later)
    // For now, just rely on external stop flag from SearchEngine
    return false;
}

} // namespace opera