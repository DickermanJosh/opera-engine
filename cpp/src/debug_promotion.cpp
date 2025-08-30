#include <iostream>
#include "Board.h"
#include "Types.h"

using namespace opera;

int main() {
    // Test promotion specifically
    std::string fen = "rnbqkbnr/ppppppP1/8/8/8/8/PPPPPPP1/RNBQKBNR w KQq - 0 1";
    Board testBoard(fen);
    
    std::cout << "Before promotion move:" << std::endl;
    std::cout << "G7: " << testBoard.getPiece(G7) << " (WHITE_PAWN = " << WHITE_PAWN << ")" << std::endl;
    std::cout << "G8: " << testBoard.getPiece(G8) << " (BLACK_KNIGHT = " << BLACK_KNIGHT << ")" << std::endl;
    testBoard.print();
    
    // Promote to queen
    Move promoteMove(G7, G8, PROMOTION, QUEEN);
    std::cout << "\nCaptured piece before move: " << testBoard.getPiece(G8) << std::endl;
    testBoard.makeMove(promoteMove);
    
    std::cout << "\nAfter promotion move:" << std::endl;
    std::cout << "G7: " << testBoard.getPiece(G7) << " (NO_PIECE = " << NO_PIECE << ")" << std::endl;
    std::cout << "G8: " << testBoard.getPiece(G8) << " (WHITE_QUEEN = " << WHITE_QUEEN << ")" << std::endl;
    testBoard.print();
    
    // Unmake promotion
    testBoard.unmakeMove(promoteMove);
    
    std::cout << "\nAfter unmake:" << std::endl;
    std::cout << "G7: " << testBoard.getPiece(G7) << " (should be WHITE_PAWN = " << WHITE_PAWN << ")" << std::endl;
    std::cout << "G8: " << testBoard.getPiece(G8) << " (should be NO_PIECE = " << NO_PIECE << ")" << std::endl;
    testBoard.print();
    
    return 0;
}