#include "search/move_ordering.h"
#include <algorithm>
#include <cstring>

namespace opera {

MoveOrdering::MoveOrdering(Board& board, TranspositionTable& tt) 
    : board(board), tt(tt) {
    
    // Initialize killer moves to null moves
    for (int depth = 0; depth < MAX_SEARCH_DEPTH; ++depth) {
        for (int i = 0; i < KILLERS_PER_DEPTH; ++i) {
            killer_moves[depth][i] = NULL_MOVE_GEN();
        }
    }
    
    // Initialize history table to zero
    for (int color = 0; color < 2; ++color) {
        for (int from = 0; from < 64; ++from) {
            for (int to = 0; to < 64; ++to) {
                history_table[color][from][to].store(0);
            }
        }
    }
}

int MoveOrdering::score_move(const MoveGen& move, int depth) {
    // 1. Check for TT move (highest priority)
    if (is_tt_move(move)) {
        return TT_MOVE_SCORE;
    }
    
    // 2. Handle captures with MVV-LVA
    if (move.isCapture()) {
        if (is_good_capture(move)) {
            return GOOD_CAPTURE_BASE + calculate_mvv_lva_score(move);
        } else {
            return BAD_CAPTURE_BASE + calculate_mvv_lva_score(move);
        }
    }
    
    // 3. Check for killer moves
    if (is_killer_move(move, depth)) {
        return KILLER_MOVE_SCORE;
    }
    
    // 4. History heuristic for quiet moves
    Color side = board.getSideToMove();
    return get_history_score(move, side);
}

int MoveOrdering::get_move_score(const MoveGen& move) const {
    uint32_t key = move_to_key(move);
    auto it = move_scores.find(key);
    return (it != move_scores.end()) ? it->second : 0;
}

void MoveOrdering::store_killer_move(const MoveGen& move, int depth) {
    if (depth < 0 || depth >= MAX_SEARCH_DEPTH) return;
    
    std::lock_guard<std::mutex> lock(killer_mutex);
    
    // Don't store captures as killers
    if (move.isCapture()) return;
    
    // Check if move is already stored as a killer
    if (killer_moves[depth][0] == move) return;
    if (killer_moves[depth][1] == move) return;
    
    // Shift killers and store new one
    killer_moves[depth][1] = killer_moves[depth][0];
    killer_moves[depth][0] = move;
}

MoveGen MoveOrdering::get_killer_move(int depth, int index) const {
    if (depth < 0 || depth >= MAX_SEARCH_DEPTH || 
        index < 0 || index >= KILLERS_PER_DEPTH) {
        return NULL_MOVE_GEN();
    }
    
    std::lock_guard<std::mutex> lock(killer_mutex);
    return killer_moves[depth][index];
}

void MoveOrdering::update_history(const MoveGen& move, Color side, int depth) {
    if (move.isCapture()) return;  // Don't update history for captures
    
    Square from = move.from();
    Square to = move.to();
    
    if (from >= 64 || to >= 64) return;  // Bounds check
    
    // Depth-based bonus (deeper searches are more valuable)
    int bonus = std::min(depth * depth, HISTORY_MAX_VALUE / 4);
    
    // Atomic update with saturation
    int current = history_table[side][from][to].load();
    int new_value = std::min(current + bonus, HISTORY_MAX_VALUE);
    history_table[side][from][to].store(new_value);
}

int MoveOrdering::get_history_score(const MoveGen& move, Color side) const {
    Square from = move.from();
    Square to = move.to();
    
    if (from >= 64 || to >= 64) return 0;  // Bounds check
    
    int history_value = history_table[side][from][to].load();
    
    // Scale history value to our scoring range
    return (history_value * HISTORY_MAX_SCORE) / HISTORY_MAX_VALUE;
}

void MoveOrdering::age_history() {
    for (int color = 0; color < 2; ++color) {
        for (int from = 0; from < 64; ++from) {
            for (int to = 0; to < 64; ++to) {
                int current = history_table[color][from][to].load();
                int aged = current / HISTORY_AGING_DIVISOR;
                history_table[color][from][to].store(aged);
            }
        }
    }
}

void MoveOrdering::clear_killers() {
    std::lock_guard<std::mutex> lock(killer_mutex);
    
    for (int depth = 0; depth < MAX_SEARCH_DEPTH; ++depth) {
        for (int i = 0; i < KILLERS_PER_DEPTH; ++i) {
            killer_moves[depth][i] = NULL_MOVE_GEN();
        }
    }
}

void MoveOrdering::clear_history() {
    for (int color = 0; color < 2; ++color) {
        for (int from = 0; from < 64; ++from) {
            for (int to = 0; to < 64; ++to) {
                history_table[color][from][to].store(0);
            }
        }
    }
}

void MoveOrdering::reset() {
    clear_killers();
    clear_history();
    move_scores.clear();
}

bool MoveOrdering::is_tt_move(const MoveGen& move) const {
    TTEntry entry;
    uint64_t zobrist_key = board.getZobristKey();
    
    if (tt.probe(zobrist_key, entry)) {
        Move tt_move = entry.get_move();
        return (tt_move.from() == move.from() && tt_move.to() == move.to());
    }
    
    return false;
}

bool MoveOrdering::is_killer_move(const MoveGen& move, int depth) const {
    if (depth < 0 || depth >= MAX_SEARCH_DEPTH) return false;
    
    std::lock_guard<std::mutex> lock(killer_mutex);
    
    for (int i = 0; i < KILLERS_PER_DEPTH; ++i) {
        if (killer_moves[depth][i] == move) {
            return true;
        }
    }
    
    return false;
}

int MoveOrdering::calculate_mvv_lva_score(const MoveGen& move) const {
    if (!move.isCapture()) return 0;
    
    // Get victim piece (piece being captured) from the move itself
    Piece victim = move.capturedPiece();
    int victim_value = get_piece_value(victim);
    
    // Get attacker piece (piece making the capture)
    Square from_square = move.from();
    Piece attacker = board.getPiece(from_square);
    int attacker_value = get_piece_value(attacker);
    
    // MVV-LVA: Most Valuable Victim - Least Valuable Attacker
    // Higher victim value = better, Lower attacker value = better
    return (victim_value * 10) - attacker_value;
}

bool MoveOrdering::is_good_capture(const MoveGen& move) const {
    if (!move.isCapture()) return false;
    
    Square from_square = move.from();
    
    Piece victim = move.capturedPiece();
    Piece attacker = board.getPiece(from_square);
    
    int victim_value = get_piece_value(victim);
    int attacker_value = get_piece_value(attacker);
    
    // Simple heuristic: capturing a more valuable piece with a less valuable piece
    // More sophisticated version would use SEE (Static Exchange Evaluation)
    if (victim_value > attacker_value) {
        return true;
    }
    
    // Equal trades are usually okay
    if (victim_value == attacker_value) {
        return true;
    }
    
    // Pawn captures are often good even if equal value
    if (typeOf(attacker) == PAWN) {
        return true;
    }
    
    // Check if the target square is defended
    // For now, assume undefended pieces are good captures
    Square to_square = move.to();
    Color opponent = ~board.getSideToMove();
    bool is_defended = board.isSquareAttacked(to_square, opponent);
    
    return !is_defended;
}

int MoveOrdering::get_piece_value(Piece piece) const {
    if (piece == NO_PIECE) return 0;
    return get_piece_type_value(typeOf(piece));
}

int MoveOrdering::get_piece_type_value(PieceType piece_type) const {
    if (piece_type > KING) {
        return 0;
    }
    return PIECE_VALUES[piece_type];
}

uint32_t MoveOrdering::move_to_key(const MoveGen& move) const {
    // Use the move's packed data as the key
    return move.hash();
}

} // namespace opera