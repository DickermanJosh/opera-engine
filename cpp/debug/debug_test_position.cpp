#include "Board.h"
#include "MoveGen.h"
#include <iostream>

using namespace opera;

int main() {
    Board board;
    
    // From BishopBlockedByOwnPieces test
    // Bishop diagonals from d4 go through: c3-b2-a1, e3-f2-g1, c5-b6-a7, e5-f6-g7-h8
    board.setFromFEN("8/1P4P1/8/8/3B4/8/1P4P1/8 w - - 0 1");
    
    std::cout << "Test position (BishopBlockedByOwnPieces):" << std::endl;
    std::cout << board.toString() << std::endl;
    
    MoveGenList<> moves;
    generateBishopMoves(board, moves, WHITE);
    
    std::cout << "Generated " << moves.size() << " bishop moves (expected 4):" << std::endl;
    for (size_t i = 0; i < moves.size(); ++i) {
        std::cout << "  " << i << ": " << moves[i].toString() 
                  << " (from=" << static_cast<int>(moves[i].from()) 
                  << ", to=" << static_cast<int>(moves[i].to()) << ")" << std::endl;
    }
    
    // Check what pieces are actually on the blocking squares
    std::cout << "\nPieces that should block bishop on diagonals:" << std::endl;
    std::cout << "B2 (square " << static_cast<int>(B2) << "): " << static_cast<int>(board.getPiece(B2)) << std::endl;
    std::cout << "G2 (square " << static_cast<int>(G2) << "): " << static_cast<int>(board.getPiece(G2)) << std::endl;
    std::cout << "B7 (square " << static_cast<int>(B7) << "): " << static_cast<int>(board.getPiece(B7)) << std::endl;
    std::cout << "G7 (square " << static_cast<int>(G7) << "): " << static_cast<int>(board.getPiece(G7)) << std::endl;
    
    return 0;
}