#include "Board.h"
#include "MoveGen.h"
#include <iostream>

using namespace opera;

int main() {
    Board board;
    board.setFromFEN("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1");
    
    std::cout << "Endgame position:" << std::endl;
    std::cout << board.toString() << std::endl;
    
    MoveGenList<> whiteMoves, blackMoves;
    generatePawnMoves(board, whiteMoves, WHITE);
    generatePawnMoves(board, blackMoves, BLACK);
    
    std::cout << "White pawn moves: " << whiteMoves.size() << std::endl;
    for (size_t i = 0; i < whiteMoves.size(); ++i) {
        std::cout << "  " << i << ": " << whiteMoves[i].toString() << std::endl;
    }
    
    std::cout << "Black pawn moves: " << blackMoves.size() << std::endl;
    for (size_t i = 0; i < blackMoves.size(); ++i) {
        std::cout << "  " << i << ": " << blackMoves[i].toString() << std::endl;
    }
    
    return 0;
}