#include <iostream>
#include "../include/Board.h"
#include "../include/MoveGen.h"

using namespace opera;

std::string squareToString(Square sq) {
    if (sq == NO_SQUARE) return "none";
    return std::string(1, 'a' + fileOf(sq)) + std::string(1, '1' + rankOf(sq));
}

int main() {
    Board board("8/8/8/8/8/8/8/K6q w - - 0 1");
    
    std::cout << "=== ILLEGAL MOVE INTO CHECK DEBUG ===" << std::endl;
    std::cout << "Position: 8/8/8/8/8/8/8/K6q w - - 0 1" << std::endl;
    std::cout << "White to move" << std::endl << std::endl;
    
    board.print();
    
    std::cout << "\n=== ANALYSIS ===" << std::endl;
    std::cout << "White king position: " << squareToString(board.getKingSquare(WHITE)) << std::endl;
    std::cout << "Black king position: " << squareToString(board.getKingSquare(BLACK)) << std::endl;
    std::cout << "White in check: " << (board.isInCheck(WHITE) ? "YES" : "NO") << std::endl;
    
    // Test the illegal move Ka1-b1
    MoveGen illegalMove(A1, B1, MoveGen::MoveType::NORMAL);
    std::cout << "\nAttempting move: " << illegalMove.toString() << std::endl;
    
    // Check if B1 is attacked by black
    std::cout << "Is B1 attacked by BLACK: " << (board.isSquareAttacked(B1, BLACK) ? "YES" : "NO") << std::endl;
    
    // Check if this move would put king in check
    std::cout << "Would this move put WHITE king in check: " << (board.wouldBeInCheck(illegalMove, WHITE) ? "YES" : "NO") << std::endl;
    
    // Try the move
    Board tempBoard = board;
    bool moveAccepted = tempBoard.makeMove(illegalMove);
    std::cout << "Move accepted by makeMove(): " << (moveAccepted ? "YES" : "NO") << std::endl;
    
    if (moveAccepted) {
        std::cout << "After move:" << std::endl;
        tempBoard.print();
        std::cout << "White in check after move: " << (tempBoard.isInCheck(WHITE) ? "YES" : "NO") << std::endl;
    }
    
    return 0;
}