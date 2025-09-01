#include "Board.h"
#include "MoveGen.h"
#include <iostream>

using namespace opera;

int main() {
    Board board;
    
    // New corrected FEN
    board.setFromFEN("8/8/1P3P2/8/3B4/8/1P3P2/8 w - - 0 1");
    
    std::cout << "Corrected test position:" << std::endl;
    std::cout << board.toString() << std::endl;
    
    MoveGenList<> moves;
    generateBishopMoves(board, moves, WHITE);
    
    std::cout << "Generated " << moves.size() << " bishop moves (expected 4):" << std::endl;
    for (size_t i = 0; i < moves.size(); ++i) {
        std::cout << "  " << i << ": " << moves[i].toString() 
                  << " (from=" << static_cast<int>(moves[i].from()) 
                  << ", to=" << static_cast<int>(moves[i].to()) << ")" << std::endl;
    }
    
    return 0;
}