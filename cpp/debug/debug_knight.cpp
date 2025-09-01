#include "Board.h"
#include "MoveGen.h"
#include <iostream>

using namespace opera;

int main() {
    Board board;
    
    // Test knight captures: "8/8/3p1p2/8/3N4/2p1p3/8/8 w - - 0 1"
    board.setFromFEN("8/8/3p1p2/8/3N4/2p1p3/8/8 w - - 0 1");
    
    std::cout << "Knight Captures Test Position:" << std::endl;
    std::cout << board.toString() << std::endl;
    
    std::cout << "Knight on D4: " << static_cast<int>(board.getPiece(D4)) << std::endl;
    std::cout << "Piece on C6: " << static_cast<int>(board.getPiece(C6)) << " (color: " << static_cast<int>(colorOf(board.getPiece(C6))) << ")" << std::endl;
    std::cout << "Piece on E6: " << static_cast<int>(board.getPiece(E6)) << " (color: " << static_cast<int>(colorOf(board.getPiece(E6))) << ")" << std::endl;
    std::cout << "Piece on F3: " << static_cast<int>(board.getPiece(F3)) << " (color: " << static_cast<int>(colorOf(board.getPiece(F3))) << ")" << std::endl;
    
    std::cout << "BLACK_PAWN value: " << static_cast<int>(BLACK_PAWN) << std::endl;
    std::cout << "NO_PIECE value: " << static_cast<int>(NO_PIECE) << std::endl;
    
    MoveGenList<> moves;
    generateKnightMoves(board, moves, WHITE);
    
    std::cout << "Generated " << moves.size() << " knight moves:" << std::endl;
    for (size_t i = 0; i < moves.size(); ++i) {
        std::cout << "  " << i << ": " << moves[i].toString() 
                  << " (from=" << static_cast<int>(moves[i].from()) 
                  << ", to=" << static_cast<int>(moves[i].to())
                  << ", captured=" << static_cast<int>(moves[i].capturedPiece())
                  << ", isCapture=" << (moves[i].isCapture() ? "true" : "false")
                  << ")" << std::endl;
    }
    
    std::cout << std::endl << "====================" << std::endl;
    
    // Test knight blocked by own pieces: "8/8/3P1P2/8/3N4/2P1P3/8/8 w - - 0 1"
    board.setFromFEN("8/8/3P1P2/8/3N4/2P1P3/8/8 w - - 0 1");
    
    std::cout << "Knight Blocked by Own Pieces Test Position:" << std::endl;
    std::cout << board.toString() << std::endl;
    
    std::cout << "Knight on D4: " << static_cast<int>(board.getPiece(D4)) << std::endl;
    std::cout << "Piece on C6: " << static_cast<int>(board.getPiece(C6)) << " (color: " << static_cast<int>(colorOf(board.getPiece(C6))) << ")" << std::endl;
    std::cout << "Piece on E6: " << static_cast<int>(board.getPiece(E6)) << " (color: " << static_cast<int>(colorOf(board.getPiece(E6))) << ")" << std::endl;
    std::cout << "Piece on F3: " << static_cast<int>(board.getPiece(F3)) << " (color: " << static_cast<int>(colorOf(board.getPiece(F3))) << ")" << std::endl;
    
    std::cout << "WHITE_PAWN value: " << static_cast<int>(WHITE_PAWN) << std::endl;
    std::cout << "WHITE color: " << static_cast<int>(WHITE) << std::endl;
    
    moves.clear();
    generateKnightMoves(board, moves, WHITE);
    
    std::cout << "Generated " << moves.size() << " knight moves:" << std::endl;
    for (size_t i = 0; i < moves.size(); ++i) {
        std::cout << "  " << i << ": " << moves[i].toString() 
                  << " (from=" << static_cast<int>(moves[i].from()) 
                  << ", to=" << static_cast<int>(moves[i].to())
                  << ", captured=" << static_cast<int>(moves[i].capturedPiece())
                  << ", isCapture=" << (moves[i].isCapture() ? "true" : "false")
                  << ")" << std::endl;
    }
    
    return 0;
}