#include "Board.h"
#include "MoveGen.h"
#include <iostream>

using namespace opera;

int main() {
    Board board;
    
    board.setFromFEN("8/8/8/3p4/1p1R1p2/8/3p4/8 w - - 0 1");
    
    std::cout << "Rook captures test position:" << std::endl;
    std::cout << board.toString() << std::endl;
    
    MoveGenList<> moves;
    generateRookMoves(board, moves, WHITE);
    
    std::cout << "Generated " << moves.size() << " rook moves (expected 8):" << std::endl;
    
    int captures = 0;
    for (size_t i = 0; i < moves.size(); ++i) {
        std::cout << "  " << i << ": " << moves[i].toString() 
                  << " (from=" << static_cast<int>(moves[i].from()) 
                  << ", to=" << static_cast<int>(moves[i].to()) 
                  << ", capture=" << (moves[i].isCapture() ? "yes" : "no") << ")" << std::endl;
        if (moves[i].isCapture()) captures++;
    }
    
    std::cout << "Capture moves: " << captures << std::endl;
    
    return 0;
}