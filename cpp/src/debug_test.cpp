#include <iostream>
#include "Board.h"
#include "Types.h"

using namespace opera;

int main() {
    // Test en passant specifically
    std::string fen = "rnbqkbnr/ppp1p1pp/8/3pPp2/8/8/PPPP1PPP/RNBQKBNR w KQkq f6 0 3";
    Board testBoard(fen);
    
    std::cout << "Before en passant move:" << std::endl;
    std::cout << "E5: " << testBoard.getPiece(E5) << " (WHITE_PAWN = " << WHITE_PAWN << ")" << std::endl;
    std::cout << "F5: " << testBoard.getPiece(F5) << " (BLACK_PAWN = " << BLACK_PAWN << ")" << std::endl;
    std::cout << "F6: " << testBoard.getPiece(F6) << " (NO_PIECE = " << NO_PIECE << ")" << std::endl;
    testBoard.print();
    
    // En passant capture: exf6
    Move epMove(E5, F6, EN_PASSANT);
    testBoard.makeMove(epMove);
    
    std::cout << "\nAfter en passant move:" << std::endl;
    std::cout << "E5: " << testBoard.getPiece(E5) << " (NO_PIECE = " << NO_PIECE << ")" << std::endl;
    std::cout << "F5: " << testBoard.getPiece(F5) << " (NO_PIECE = " << NO_PIECE << ")" << std::endl;
    std::cout << "F6: " << testBoard.getPiece(F6) << " (WHITE_PAWN = " << WHITE_PAWN << ")" << std::endl;
    testBoard.print();
    
    // Debug the unmake operation
    std::cout << "\nBefore unmake, sideToMove = " << testBoard.getSideToMove() << std::endl;
    
    // Unmake en passant
    testBoard.unmakeMove(epMove);
    
    std::cout << "\nAfter unmake:" << std::endl;
    std::cout << "E5: " << testBoard.getPiece(E5) << " (should be WHITE_PAWN = " << WHITE_PAWN << ")" << std::endl;
    std::cout << "F5: " << testBoard.getPiece(F5) << " (should be BLACK_PAWN = " << BLACK_PAWN << ")" << std::endl;
    std::cout << "F6: " << testBoard.getPiece(F6) << " (should be NO_PIECE = " << NO_PIECE << ")" << std::endl;
    std::cout << "After unmake, sideToMove = " << testBoard.getSideToMove() << std::endl;
    testBoard.print();
    
    return 0;
}