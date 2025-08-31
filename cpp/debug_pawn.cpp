#include "Board.h"
#include "MoveGen.h"
#include <iostream>
#include <vector>
#include <string>

using namespace opera;

int main() {
    std::vector<std::string> testPositions = {
        // Test promotion capture position
        "rnbqkbnr/pppppPpp/8/8/8/8/PPPPPpPP/RNBQKB1R w KQkq - 0 1",
        // Test black promotion
        "1nbqkb1r/pppppppp/8/8/8/8/pppppPpp/RNBQKBNR b KQkq - 0 1"
    };
    
    for (size_t i = 0; i < testPositions.size(); ++i) {
        Board board;
        board.setFromFEN(testPositions[i]);
        
        std::cout << "=== Position " << i << " ===" << std::endl;
        std::cout << "FEN: " << testPositions[i] << std::endl;
        std::cout << board.toString() << std::endl;
        
        if (i == 0) {
            // Test white promotion capture
            std::cout << "Piece on G8: " << static_cast<int>(board.getPiece(G8)) << " (BLACK_ROOK=" << BLACK_ROOK << ", BLACK_KNIGHT=" << BLACK_KNIGHT << ")" << std::endl;
            MoveGenList<> whiteMoves;
            generatePawnMoves(board, whiteMoves, WHITE);
            
            for (size_t j = 0; j < whiteMoves.size(); ++j) {
                const auto& move = whiteMoves[j];
                if (move.from() == F7 && move.to() == G8 && move.isPromotion()) {
                    std::cout << "F7-G8 promotion: captured=" << static_cast<int>(move.capturedPiece()) << std::endl;
                }
            }
        } else if (i == 1) {
            // Test black promotion
            MoveGenList<> blackMoves;
            generatePawnMoves(board, blackMoves, BLACK);
            std::cout << "Black pawn moves: " << blackMoves.size() << std::endl;
            for (size_t j = 0; j < blackMoves.size(); ++j) {
                const auto& move = blackMoves[j];
                std::cout << "  " << j << ": " << move.toString() 
                          << " from=" << move.from() << " to=" << move.to() << std::endl;
            }
        }
        std::cout << std::endl;
    }
    
    return 0;
}