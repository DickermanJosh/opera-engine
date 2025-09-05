#pragma once

#include "Types.h"
#include "MoveGen.h"
#include <vector>

namespace opera {

// Forward declaration
class Board;

/**
 * @brief Static Exchange Evaluation (SEE) for accurate capture analysis
 * 
 * Evaluates the material outcome of a capture sequence by calculating
 * all possible recaptures until no more captures are profitable.
 * Essential for move ordering and quiescence search quality.
 */
class StaticExchangeEvaluator {
public:
    /**
     * @brief Construct SEE evaluator for given board position
     * @param board Reference to board position for evaluation
     */
    explicit StaticExchangeEvaluator(const Board& board);
    
    /**
     * @brief Evaluate material outcome of a capture sequence
     * @param move The initial capture move to evaluate
     * @return Net material change (positive = material gain, negative = material loss)
     */
    int evaluate(const MoveGen& move);
    
    /**
     * @brief Quick SEE evaluation for move ordering (optimized version)
     * @param move The capture move to evaluate
     * @return Approximate material outcome (faster than full evaluate)
     */
    int quick_evaluate(const MoveGen& move);
    
    /**
     * @brief Check if a capture is profitable (SEE >= threshold)
     * @param move The capture move to check
     * @param threshold Minimum material gain required (default: 0)
     * @return True if capture is profitable, false otherwise
     */
    bool is_good_capture(const MoveGen& move, int threshold = 0);

private:
    const Board& board;
    
    // Piece values for SEE calculation (indexed by PieceType enum)
    static constexpr int PIECE_VALUES[7] = {
        100,   // PAWN [0]
        320,   // KNIGHT [1]
        330,   // BISHOP [2]  
        500,   // ROOK [3]
        900,   // QUEEN [4]
        20000, // KING [5]
        0      // NO_PIECE_TYPE [6]
    };
    
    /**
     * @brief Get all attackers of a square, sorted by piece value (least valuable first)
     * @param square Target square to find attackers for
     * @param color Color of attacking side
     * @return Vector of attacking piece squares, sorted by value
     */
    std::vector<Square> get_attackers(Square square, Color color) const;
    
    
    /**
     * @brief Get all attackers from both sides for a square
     * @param square Target square
     * @param white_attackers Output vector for white attackers
     * @param black_attackers Output vector for black attackers  
     */
    void get_all_attackers(Square square, 
                          std::vector<Square>& white_attackers,
                          std::vector<Square>& black_attackers) const;
    
    /**
     * @brief Get the least valuable attacker for a color
     * @param square Target square
     * @param color Attacking color
     * @return Square of least valuable attacker, or SQUARE_NONE if none
     */
    Square get_least_valuable_attacker(Square square, Color color) const;
    
    /**
     * @brief Get least valuable attacker excluding already used pieces (for SEE simulation)
     * @param square Target square
     * @param color Attacking side color
     * @param excluded_squares Squares to exclude from consideration (already captured pieces)
     * @return Square of least valuable attacker, or SQUARE_NONE if no attackers
     */
    Square get_least_valuable_attacker_excluding(Square square, Color color, const std::vector<Square>& excluded_squares) const;
    
    /**
     * @brief Check if a piece is pinned and cannot move to target square
     * @param from Source square of piece
     * @param to Target square 
     * @param piece_color Color of the piece
     * @return True if piece is pinned and cannot make the move
     */
    bool is_pinned(Square from, Square to, Color piece_color) const;
    
    /**
     * @brief Get piece value for SEE calculation
     * @param piece Piece to get value for
     * @return Material value of piece
     */
    int get_piece_value(Piece piece) const;
    
    /**
     * @brief Get piece type value for SEE calculation
     * @param piece_type Piece type to get value for
     * @return Material value of piece type
     */
    int get_piece_type_value(PieceType piece_type) const;
    
    /**
     * @brief Check if a square is attacked by pieces of given color
     * @param square Target square
     * @param color Attacking color
     * @return True if square is under attack
     */
    bool is_square_attacked(Square square, Color color) const;
    
    /**
     * @brief Find X-ray attackers (sliding pieces behind other pieces)
     * @param square Target square
     * @param color Attacking color
     * @param blocker_square Square of piece that was removed (revealing X-ray)
     * @return Vector of X-ray attacking piece squares
     */
    std::vector<Square> get_xray_attackers(Square square, Color color, Square blocker_square) const;
    
    /**
     * @brief Simulate making a move temporarily for SEE calculation
     * @param from Source square
     * @param to Target square
     * @param captured Piece that was captured (for restoration)
     * @return True if move was made successfully
     */
    bool make_see_move(Square from, Square to, Piece& captured);
    
    /**
     * @brief Restore board state after SEE move simulation
     * @param from Source square  
     * @param to Target square
     * @param captured Piece that was captured
     */
    void unmake_see_move(Square from, Square to, Piece captured);
    
    /**
     * @brief Core SEE algorithm - recursive exchange calculation
     * @param square Target square of exchange
     * @param target_piece Piece being captured initially
     * @param attacking_color Color of side making next capture
     * @param depth Current depth in exchange sequence (for cycle detection)
     * @return Material outcome from this point in exchange
     */
    int see_recursive(Square square, Piece target_piece, Color attacking_color, int depth = 0);
    
    /**
     * @brief Alternative iterative SEE algorithm (may be faster for complex positions)
     * @param move Initial capture move
     * @return Material outcome of exchange sequence
     */
    int see_iterative(const MoveGen& move);
    
    /**
     * @brief Check if a piece can attack a square (helper function)
     * @param piece The piece to check
     * @param from Source square of piece
     * @param to Target square to attack
     * @return True if piece can attack target square
     */
    bool can_piece_attack(Piece piece, Square from, Square to) const;
    
    /**
     * @brief Check if two squares are on the same diagonal
     * @param a First square
     * @param b Second square
     * @return True if squares are on same diagonal
     */
    bool are_squares_on_same_diagonal(Square a, Square b) const;
    
    /**
     * @brief Check if two squares are on the same rank or file
     * @param a First square
     * @param b Second square
     * @return True if squares are on same rank or file
     */
    bool are_squares_on_same_rank_or_file(Square a, Square b) const;
    
    /**
     * @brief Check if path between two squares is blocked by pieces
     * @param from Starting square
     * @param to Target square  
     * @return True if path is blocked by pieces
     */
    bool is_path_blocked(Square from, Square to) const;
};

} // namespace opera