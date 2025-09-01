#include "Board.h"
#include "MoveGen.h"
#include <iostream>

using namespace opera;

int main() {
    Board board;
    
    board.setFromFEN("8/8/8/3P4/1P1R1P2/8/3P4/8 w - - 0 1");
    
    std::cout << "Rook blocked test position:" << std::endl;
    std::cout << board.toString() << std::endl;
    
    MoveGenList<> moves;
    generateRookMoves(board, moves, WHITE);
    
    std::cout << "Generated " << moves.size() << " rook moves (expected 4):" << std::endl;
    for (size_t i = 0; i < moves.size(); ++i) {
        std::cout << "  " << i << ": " << moves[i].toString() 
                  << " (from=" << static_cast<int>(moves[i].from()) 
                  << ", to=" << static_cast<int>(moves[i].to()) << ")" << std::endl;
    }
    
    // Check what pieces are blocking
    std::cout << "\nPieces around D4:" << std::endl;
    std::cout << "B4 (left-left): " << static_cast<int>(board.getPiece(B4)) << std::endl;
    std::cout << "C4 (left): " << static_cast<int>(board.getPiece(C4)) << std::endl;
    std::cout << "E4 (right): " << static_cast<int>(board.getPiece(E4)) << std::endl;
    std::cout << "F4 (right-right): " << static_cast<int>(board.getPiece(F4)) << std::endl;
    std::cout << "D2 (down-down): " << static_cast<int>(board.getPiece(D2)) << std::endl;
    std::cout << "D3 (down): " << static_cast<int>(board.getPiece(D3)) << std::endl;
    std::cout << "D5 (up): " << static_cast<int>(board.getPiece(D5)) << std::endl;
    std::cout << "D6 (up-up): " << static_cast<int>(board.getPiece(D6)) << std::endl;
    
    return 0;
}