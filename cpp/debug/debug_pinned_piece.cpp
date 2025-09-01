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
    Board board("8/8/8/8/8/8/8/r2B3K w - - 0 1");
    
    std::cout << "=== PINNED PIECE DEBUG ===" << std::endl;
    std::cout << "Position: 8/8/8/8/8/8/8/r2B3K w - - 0 1" << std::endl;
    std::cout << "White to move, testing WHITE bishop pinned by black rook" << std::endl << std::endl;
    
    board.print();
    
    std::cout << "\n=== ANALYSIS ===" << std::endl;
    std::cout << "White king position: " << squareToString(board.getKingSquare(WHITE)) << std::endl;
    std::cout << "Black king position: " << squareToString(board.getKingSquare(BLACK)) << std::endl;
    std::cout << "White in check: " << (board.isInCheck(WHITE) ? "YES" : "NO") << std::endl;
    std::cout << "Black in check: " << (board.isInCheck(BLACK) ? "YES" : "NO") << std::endl;
    
    // Generate bishop moves for white
    MoveGenList<> bishopMoves;
    generateBishopMoves(board, bishopMoves, WHITE);
    
    std::cout << "\n=== WHITE BISHOP MOVES ===" << std::endl;
    std::cout << "Total pseudo-legal moves: " << bishopMoves.size() << std::endl;
    
    for (size_t i = 0; i < bishopMoves.size(); ++i) {
        if (bishopMoves[i].from() == D1) {
            std::cout << "Bishop move " << (i+1) << ": " << bishopMoves[i].toString() << std::endl;
        }
    }
    
    // Check each bishop move for legality
    std::cout << "\n=== LEGALITY CHECK ===" << std::endl;
    int legalBishopMoves = 0;
    for (size_t i = 0; i < bishopMoves.size(); ++i) {
        if (bishopMoves[i].from() == D1) {
            Board tempBoard = board;
            bool moveAccepted = tempBoard.makeMove(bishopMoves[i]);
            bool kingInCheck = false;
            if (moveAccepted) {
                kingInCheck = tempBoard.isInCheck(WHITE);
            }
            
            std::cout << "Move " << bishopMoves[i].toString() 
                      << ": makeMove=" << (moveAccepted ? "YES" : "NO")
                      << ", kingincheck=" << (kingInCheck ? "YES" : "NO");
            
            if (moveAccepted && !kingInCheck) {
                legalBishopMoves++;
                std::cout << " -> LEGAL";
            } else {
                std::cout << " -> ILLEGAL";
            }
            std::cout << std::endl;
        }
    }
    
    std::cout << "\nTotal legal bishop moves: " << legalBishopMoves << std::endl;
    
    return 0;
}