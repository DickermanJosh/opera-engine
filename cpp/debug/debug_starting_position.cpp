#include "Board.h"
#include "MoveGen.h"
#include <iostream>

using namespace opera;

int main() {
    Board board;
    board.setFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    
    std::cout << "Starting position board:" << std::endl;
    std::cout << board.toString() << std::endl;
    
    MoveGenList<> moves;
    generateBishopMoves(board, moves, WHITE);
    
    std::cout << "White bishop moves generated: " << moves.size() << std::endl;
    
    for (size_t i = 0; i < moves.size(); ++i) {
        std::cout << "Move " << i << ": " << moves[i].toString() 
                  << " (from=" << static_cast<int>(moves[i].from()) 
                  << ", to=" << static_cast<int>(moves[i].to()) << ")" << std::endl;
    }
    
    // Check white bishop positions in starting position
    std::cout << "\nWhite bishops in starting position:" << std::endl;
    std::cout << "Bishop on C1: " << static_cast<int>(board.getPiece(C1)) << std::endl;
    std::cout << "Bishop on F1: " << static_cast<int>(board.getPiece(F1)) << std::endl;
    
    // Check what pieces are blocking the bishops
    std::cout << "\nPieces that should block bishops:" << std::endl;
    std::cout << "Piece on B2 (should block C1 bishop): " << static_cast<int>(board.getPiece(B2)) << std::endl;
    std::cout << "Piece on D2 (should block C1 bishop): " << static_cast<int>(board.getPiece(D2)) << std::endl;
    std::cout << "Piece on E2 (should block F1 bishop): " << static_cast<int>(board.getPiece(E2)) << std::endl;
    std::cout << "Piece on G2 (should block F1 bishop): " << static_cast<int>(board.getPiece(G2)) << std::endl;
    
    return 0;
}