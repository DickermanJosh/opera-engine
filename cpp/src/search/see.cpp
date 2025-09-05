#include "search/see.h"
#include "Board.h"
#include <algorithm>
#include <climits>
#include <iostream>

namespace opera {

// Define SQUARE_NONE constant
constexpr Square SQUARE_NONE = static_cast<Square>(255);

StaticExchangeEvaluator::StaticExchangeEvaluator(const Board& board) : board(board) {
}

int StaticExchangeEvaluator::evaluate(const MoveGen& move) {
    // Handle non-capture moves
    if (!move.isCapture()) {
        return 0;
    }
    
    // Use iterative algorithm for main evaluation
    return see_iterative(move);
}

int StaticExchangeEvaluator::quick_evaluate(const MoveGen& move) {
    // Quick version - just check if immediate capture is profitable
    if (!move.isCapture()) {
        return 0;
    }
    
    Square from = move.from();
    Square to = move.to();
    Piece attacker = board.getPiece(from);
    Piece victim = move.capturedPiece();
    
    if (attacker == NO_PIECE || victim == NO_PIECE) {
        return 0;
    }
    
    int victim_value = get_piece_value(victim);
    int attacker_value = get_piece_value(attacker);
    
    // Quick heuristic: if target square is not defended, capture is good
    Color defender_color = ~board.getSideToMove();
    if (!is_square_attacked(to, defender_color)) {
        return victim_value;
    }
    
    // Simple approximation: victim value - attacker value
    return victim_value - attacker_value;
}

bool StaticExchangeEvaluator::is_good_capture(const MoveGen& move, int threshold) {
    int see_value = evaluate(move);
    return see_value >= threshold;
}

std::vector<Square> StaticExchangeEvaluator::get_attackers(Square square, Color color) const {
    std::vector<Square> attackers;
    
    // Check all squares for pieces that attack the target square
    for (int sq = 0; sq < 64; ++sq) {
        Square from_square = static_cast<Square>(sq);
        Piece piece = board.getPiece(from_square);
        
        if (piece == NO_PIECE || colorOf(piece) != color) {
            continue;
        }
        
        // Check if this specific piece can attack the target square
        if (can_piece_attack(piece, from_square, square)) {
            attackers.push_back(from_square);
        }
    }
    
    // Sort by piece value (least valuable first for SEE)
    std::sort(attackers.begin(), attackers.end(), [this](Square a, Square b) {
        Piece piece_a = board.getPiece(a);
        Piece piece_b = board.getPiece(b);
        return get_piece_value(piece_a) < get_piece_value(piece_b);
    });
    
    return attackers;
}

void StaticExchangeEvaluator::get_all_attackers(Square square, 
                                               std::vector<Square>& white_attackers,
                                               std::vector<Square>& black_attackers) const {
    white_attackers = get_attackers(square, WHITE);
    black_attackers = get_attackers(square, BLACK);
}

Square StaticExchangeEvaluator::get_least_valuable_attacker(Square square, Color color) const {
    std::vector<Square> attackers = get_attackers(square, color);
    
    if (attackers.empty()) {
        return SQUARE_NONE;
    }
    
    // Already sorted by value, so first is least valuable
    return attackers[0];
}

Square StaticExchangeEvaluator::get_least_valuable_attacker_excluding(Square square, Color color, const std::vector<Square>& excluded_squares) const {
    std::vector<Square> attackers = get_attackers(square, color);
    
    // Filter out excluded squares
    attackers.erase(std::remove_if(attackers.begin(), attackers.end(),
        [&excluded_squares](Square sq) {
            return std::find(excluded_squares.begin(), excluded_squares.end(), sq) != excluded_squares.end();
        }), attackers.end());
    
    if (attackers.empty()) {
        return SQUARE_NONE;
    }
    
    // Already sorted by value, so first is least valuable
    return attackers[0];
}

bool StaticExchangeEvaluator::is_pinned(Square from, Square /* to */, Color piece_color) const {
    // Find the king of the same color
    Square king_square = board.getKingSquare(piece_color);
    
    if (king_square == SQUARE_NONE) {
        return false; // No king found, can't be pinned
    }
    
    // Check if moving this piece would expose the king to attack
    // This is a simplified check - full implementation would need to simulate the move
    
    // Check if there's a sliding piece attacking through the 'from' square to the king
    Color opponent = ~piece_color;
    
    // Check diagonals (bishops, queens)
    if (are_squares_on_same_diagonal(from, king_square)) {
        // Look for enemy bishops/queens on the diagonal
        int delta_rank = (king_square / 8) - (from / 8);
        int delta_file = (king_square % 8) - (from % 8);
        
        // Normalize direction
        int rank_step = (delta_rank > 0) ? 1 : (delta_rank < 0) ? -1 : 0;
        int file_step = (delta_file > 0) ? 1 : (delta_file < 0) ? -1 : 0;
        
        // Look beyond the king for attacking pieces
        Square check_square = static_cast<Square>(king_square + rank_step * 8 + file_step);
        while (check_square >= 0 && check_square < 64) {
            Piece piece = board.getPiece(check_square);
            if (piece != NO_PIECE) {
                if (colorOf(piece) == opponent && 
                    (typeOf(piece) == BISHOP || typeOf(piece) == QUEEN)) {
                    return true; // Piece is pinned
                }
                break; // Found a piece, no pin
            }
            check_square = static_cast<Square>(check_square + rank_step * 8 + file_step);
            if (check_square < 0 || check_square >= 64) break;
        }
    }
    
    // Check ranks and files (rooks, queens)
    if (are_squares_on_same_rank_or_file(from, king_square)) {
        // Similar logic for rooks/queens on ranks/files
        // Simplified implementation
        return false; // For now, assume not pinned
    }
    
    return false;
}

int StaticExchangeEvaluator::get_piece_value(Piece piece) const {
    if (piece == NO_PIECE) return 0;
    return get_piece_type_value(typeOf(piece));
}

int StaticExchangeEvaluator::get_piece_type_value(PieceType piece_type) const {
    if (piece_type > KING) return 0;
    return PIECE_VALUES[piece_type];
}

bool StaticExchangeEvaluator::is_square_attacked(Square square, Color color) const {
    return board.isSquareAttacked(square, color);
}

std::vector<Square> StaticExchangeEvaluator::get_xray_attackers(Square /* square */, Color /* color */, Square /* blocker_square */) const {
    std::vector<Square> xray_attackers;
    
    // This is a complex operation - look for sliding pieces that would attack
    // the square if the blocker piece were removed
    // Simplified implementation for now
    
    return xray_attackers;
}

bool StaticExchangeEvaluator::make_see_move(Square /* from */, Square to, Piece& captured) {
    captured = board.getPiece(to);
    
    // This would need to modify board state temporarily
    // For now, just store the captured piece
    return true;
}

void StaticExchangeEvaluator::unmake_see_move(Square /* from */, Square /* to */, Piece /* captured */) {
    // Restore board state after temporary move
    // For now, this is a placeholder
}

int StaticExchangeEvaluator::see_recursive(Square square, Piece target_piece, Color attacking_color, int depth) {
    // Prevent infinite recursion
    if (depth > 16) {
        return 0;
    }
    
    // Find least valuable attacker
    Square attacker_square = get_least_valuable_attacker(square, attacking_color);
    
    if (attacker_square == SQUARE_NONE) {
        // No attackers left, exchange ends
        return 0;
    }
    
    Piece attacker = board.getPiece(attacker_square);
    if (attacker == NO_PIECE) {
        return 0;
    }
    
    // Check if attacker is pinned
    if (is_pinned(attacker_square, square, attacking_color)) {
        return 0;
    }
    
    int target_value = get_piece_value(target_piece);
    
    // Recursively calculate what opponent can win back
    int opponent_gain = see_recursive(square, attacker, ~attacking_color, depth + 1);
    
    // Current side gains target piece value minus what opponent can win back
    return target_value - opponent_gain;
}

int StaticExchangeEvaluator::see_iterative(const MoveGen& move) {
    Square to = move.to();
    Square from = move.from();
    Piece initial_attacker = board.getPiece(from);
    Piece target_piece = move.capturedPiece();
    
    if (initial_attacker == NO_PIECE || target_piece == NO_PIECE) {
        return 0;
    }
    
    // Handle special cases
    if (move.isEnPassant()) {
        return get_piece_type_value(PAWN);
    }
    
    if (move.isPromotion()) {
        Piece promotion_piece = move.promotionPiece();
        int promotion_bonus = get_piece_value(promotion_piece) - get_piece_type_value(PAWN);
        return get_piece_value(target_piece) + promotion_bonus;
    }
    
    // Improved SEE using gain array approach (Stockfish-inspired)
    std::vector<int> gain_list;
    gain_list.push_back(get_piece_value(target_piece));
    
    // Track used attackers to avoid infinite loops
    std::vector<Square> used_squares;
    used_squares.push_back(from); // Initial attacker is "used"
    
    // Simulate the exchange sequence
    Piece piece_on_square = initial_attacker;
    Color side_to_move = ~colorOf(initial_attacker); // Opponent moves next
    
    
    // Continue exchange sequence while both sides have attackers
    while (true) {
        Square next_attacker = get_least_valuable_attacker_excluding(to, side_to_move, used_squares);
        
        if (next_attacker == SQUARE_NONE) {
            break; // No more attackers for this side
        }
        
        Piece attacking_piece = board.getPiece(next_attacker);
        if (attacking_piece == NO_PIECE) {
            break;
        }
        
        // Add the value of the piece being captured to gain list
        int capture_value = get_piece_value(piece_on_square);
        gain_list.push_back(capture_value);
        piece_on_square = attacking_piece;
        
        // Mark this attacker as used
        used_squares.push_back(next_attacker);
        
        // Switch sides for next iteration
        side_to_move = ~side_to_move;
        
        // Safety limit to avoid infinite loops
        if (gain_list.size() > 10) {
            break;
        }
    }
    
    // Use minimax to calculate final result from gain list
    // Work backwards through the gain list
    int result = 0;
    for (int i = gain_list.size() - 1; i >= 0; --i) {
        result = gain_list[i] - result;
    }
    
    return result;
}

// Helper functions (these would typically be in Board or utility file)
bool StaticExchangeEvaluator::can_piece_attack(Piece piece, Square from, Square to) const {
    PieceType piece_type = typeOf(piece);
    
    switch (piece_type) {
        case PAWN: {
            Color color = colorOf(piece);
            int direction = (color == WHITE) ? 1 : -1;
            int from_rank = from / 8;
            int from_file = from % 8;
            int to_rank = to / 8;
            int to_file = to % 8;
            
                // Pawn captures diagonally
            if (to_rank == from_rank + direction && 
                (to_file == from_file + 1 || to_file == from_file - 1)) {
                return true;
            }
            return false;
        }
        
        case KNIGHT: {
            int rank_diff = abs((to / 8) - (from / 8));
            int file_diff = abs((to % 8) - (from % 8));
            return (rank_diff == 2 && file_diff == 1) || (rank_diff == 1 && file_diff == 2);
        }
        
        case BISHOP:
            return are_squares_on_same_diagonal(from, to) && !is_path_blocked(from, to);
            
        case ROOK:
            return are_squares_on_same_rank_or_file(from, to) && !is_path_blocked(from, to);
            
        case QUEEN:
            return (are_squares_on_same_diagonal(from, to) || are_squares_on_same_rank_or_file(from, to)) 
                   && !is_path_blocked(from, to);
            
        case KING: {
            int rank_diff = abs((to / 8) - (from / 8));
            int file_diff = abs((to % 8) - (from % 8));
            return rank_diff <= 1 && file_diff <= 1;
        }
        
        default:
            return false;
    }
}

bool StaticExchangeEvaluator::are_squares_on_same_diagonal(Square a, Square b) const {
    int rank_diff = abs((a / 8) - (b / 8));
    int file_diff = abs((a % 8) - (b % 8));
    return rank_diff == file_diff && rank_diff > 0;
}

bool StaticExchangeEvaluator::are_squares_on_same_rank_or_file(Square a, Square b) const {
    return (a / 8 == b / 8) || (a % 8 == b % 8);
}

bool StaticExchangeEvaluator::is_path_blocked(Square from, Square to) const {
    if (from == to) return false;
    
    int from_rank = from / 8;
    int from_file = from % 8;
    int to_rank = to / 8;
    int to_file = to % 8;
    
    int rank_delta = to_rank - from_rank;
    int file_delta = to_file - from_file;
    
    // Normalize direction
    int rank_step = (rank_delta > 0) ? 1 : (rank_delta < 0) ? -1 : 0;
    int file_step = (file_delta > 0) ? 1 : (file_delta < 0) ? -1 : 0;
    
    // Check each square in the path (excluding start and end)
    Square current = static_cast<Square>(from + rank_step * 8 + file_step);
    
    while (current != to) {
        if (current < 0 || current >= 64) break; // Out of bounds
        
        if (board.getPiece(current) != NO_PIECE) {
            return true; // Path is blocked
        }
        
        current = static_cast<Square>(current + rank_step * 8 + file_step);
        
        // Safety check to prevent infinite loops
        if (abs(static_cast<int>(current) - static_cast<int>(from)) > 8) {
            break;
        }
    }
    
    return false;
}

} // namespace opera