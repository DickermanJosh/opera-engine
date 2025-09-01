#include "Board.h"
#include "MoveGen.h"
#include <iostream>

using namespace opera;

int main() {
    Board board;
    
    board.setFromFEN("8/8/8/8/3B4/8/8/B6B w - - 0 1");
    
    std::cout << "Multiple bishops position:" << std::endl;
    std::cout << board.toString() << std::endl;
    
    MoveGenList<> moves;
    generateBishopMoves(board, moves, WHITE);
    
    std::cout << "Generated " << moves.size() << " bishop moves (expected 27):" << std::endl;
    
    // Count moves from each bishop
    int d4_moves = 0, a1_moves = 0, h1_moves = 0;
    for (size_t i = 0; i < moves.size(); ++i) {
        Square from = moves[i].from();
        if (from == D4) d4_moves++;
        else if (from == A1) a1_moves++;
        else if (from == H1) h1_moves++;
        
        std::cout << "  " << i << ": " << moves[i].toString() 
                  << " (from=" << static_cast<int>(moves[i].from()) 
                  << ", to=" << static_cast<int>(moves[i].to()) << ")" << std::endl;
    }
    
    std::cout << "D4 moves: " << d4_moves << " (expected 13)" << std::endl;
    std::cout << "A1 moves: " << a1_moves << " (expected 7)" << std::endl;
    std::cout << "H1 moves: " << h1_moves << " (expected 7)" << std::endl;
    
    return 0;
}