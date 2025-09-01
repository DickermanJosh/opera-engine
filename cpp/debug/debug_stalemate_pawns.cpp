#include <iostream>
#include "../include/Board.h"
#include "../include/MoveGen.h"
#include "../src/board/MoveGenerator.cpp"

using namespace opera;

std::string squareToString(Square sq) {
    if (sq == NO_SQUARE) return "none";
    return std::string(1, 'a' + fileOf(sq)) + std::string(1, '1' + rankOf(sq));
}

int main() {
    Board board("k7/P7/1K6/8/8/8/8/8 b - - 0 1");
    
    // Also test the old position for comparison
    Board oldBoard("8/8/8/8/8/5k2/5P2/5K2 b - - 0 1");
    
    std::cout << "=== STALEMATE WITH PAWNS DEBUG ===" << std::endl;
    std::cout << "Position: k7/P7/1K6/8/8/8/8/8 b - - 0 1" << std::endl;
    std::cout << "Black to move" << std::endl << std::endl;
    
    board.print();
    
    std::cout << "\n=== ANALYSIS ===" << std::endl;
    std::cout << "Black king position: " << squareToString(board.getKingSquare(BLACK)) << std::endl;
    std::cout << "White king position: " << squareToString(board.getKingSquare(WHITE)) << std::endl;
    std::cout << "Black in check: " << (board.isInCheck(BLACK) ? "YES" : "NO") << std::endl;
    
    // Generate all legal moves for black
    MoveGenList<> legalMoves;
    generateAllLegalMoves(board, legalMoves, BLACK);
    
    std::cout << "\n=== BLACK LEGAL MOVES ===" << std::endl;
    std::cout << "Total legal moves: " << legalMoves.size() << std::endl;
    
    for (size_t i = 0; i < legalMoves.size(); ++i) {
        std::cout << "Move " << (i+1) << ": " << legalMoves[i].toString() << std::endl;
    }
    
    // Let's check king moves specifically
    std::cout << "\n=== BLACK KING MOVES ANALYSIS ===" << std::endl;
    
    Square kingPos = board.getKingSquare(BLACK);
    
    // Check all possible king squares
    Square possibleMoves[] = {
        static_cast<Square>(kingPos - 9), // up-left
        static_cast<Square>(kingPos - 8), // up
        static_cast<Square>(kingPos - 7), // up-right
        static_cast<Square>(kingPos - 1), // left
        static_cast<Square>(kingPos + 1), // right
        static_cast<Square>(kingPos + 7), // down-left
        static_cast<Square>(kingPos + 8), // down
        static_cast<Square>(kingPos + 9)  // down-right
    };
    
    for (int i = 0; i < 8; i++) {
        Square toSquare = possibleMoves[i];
        if (isValidSquare(toSquare) && std::abs(fileOf(toSquare) - fileOf(kingPos)) <= 1) {
            std::cout << "Square " << squareToString(toSquare) << ": ";
            
            if (board.isOccupied(toSquare)) {
                std::cout << "OCCUPIED" << std::endl;
                continue;
            }
            
            // Check if moving here would be in check
            MoveGen testMove(kingPos, toSquare, MoveGen::MoveType::NORMAL);
            if (board.wouldBeInCheck(testMove, BLACK)) {
                std::cout << "WOULD BE IN CHECK (controlled by white)" << std::endl;
            } else {
                std::cout << "LEGAL MOVE!" << std::endl;
            }
        }
    }
    
    std::cout << "\n=== STALEMATE CHECK ===" << std::endl;
    std::cout << "Is stalemate: " << (board.isStalemate(BLACK) ? "YES" : "NO") << std::endl;
    std::cout << "Is draw: " << (board.isDraw() ? "YES" : "NO") << std::endl;
    
    return 0;
}