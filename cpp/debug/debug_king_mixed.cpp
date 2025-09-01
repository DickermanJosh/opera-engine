#include "Board.h"
#include "MoveGen.h"
#include <iostream>

using namespace opera;

int main() {
    Board board;
    
    // From KingMixedPiecesAround test
    board.setFromFEN("8/8/8/2pPP3/3K1p2/2P1P3/8/8 w - - 0 1");
    
    std::cout << "King mixed pieces test position:" << std::endl;
    std::cout << board.toString() << std::endl;
    
    MoveGenList<> moves;
    generateKingMoves(board, moves, WHITE);
    
    std::cout << "Generated " << moves.size() << " king moves (expected 4):" << std::endl;
    
    int captures = 0;
    for (size_t i = 0; i < moves.size(); ++i) {
        std::cout << "  " << i << ": " << moves[i].toString() 
                  << " (from=" << static_cast<int>(moves[i].from()) 
                  << ", to=" << static_cast<int>(moves[i].to()) 
                  << ", capture=" << (moves[i].isCapture() ? "yes" : "no") << ")" << std::endl;
        if (moves[i].isCapture()) captures++;
    }
    
    std::cout << "Capture moves: " << captures << " (expected 2)" << std::endl;
    
    // Check what pieces are around the king on D4
    std::cout << "\nPieces around D4:" << std::endl;
    std::cout << "C3: " << static_cast<int>(board.getPiece(C3)) << std::endl;
    std::cout << "C4: " << static_cast<int>(board.getPiece(C4)) << std::endl;
    std::cout << "C5: " << static_cast<int>(board.getPiece(C5)) << std::endl;
    std::cout << "D3: " << static_cast<int>(board.getPiece(D3)) << std::endl;
    std::cout << "D5: " << static_cast<int>(board.getPiece(D5)) << std::endl;
    std::cout << "E3: " << static_cast<int>(board.getPiece(E3)) << std::endl;
    std::cout << "E4: " << static_cast<int>(board.getPiece(E4)) << std::endl;
    std::cout << "E5: " << static_cast<int>(board.getPiece(E5)) << std::endl;
    
    return 0;
}