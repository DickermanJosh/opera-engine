#include <iostream>
#include "../include/Board.h"
#include "../include/MoveGen.h"

using namespace opera;

std::string squareToString(Square sq) {
    if (sq == NO_SQUARE) return "none";
    return std::string(1, 'a' + fileOf(sq)) + std::string(1, '1' + rankOf(sq));
}

int main() {
    Board board("r3k2r/8/8/8/8/8/5q2/R3K2R w KQkq - 0 1");
    
    std::cout << "=== CASTLING THROUGH CHECK DEBUG ===" << std::endl;
    std::cout << "Position: r3k2r/8/8/8/8/8/5q2/R3K2R w KQkq - 0 1" << std::endl;
    std::cout << "White to move, BLACK queen attacks f1" << std::endl << std::endl;
    
    board.print();
    
    std::cout << "\n=== ANALYSIS ===" << std::endl;
    std::cout << "White king position: " << squareToString(board.getKingSquare(WHITE)) << std::endl;
    std::cout << "Black king position: " << squareToString(board.getKingSquare(BLACK)) << std::endl;
    std::cout << "White in check: " << (board.isInCheck(WHITE) ? "YES" : "NO") << std::endl;
    std::cout << "Black in check: " << (board.isInCheck(BLACK) ? "YES" : "NO") << std::endl;
    
    // Check castling rights
    std::cout << "White can castle kingside: " << (board.canCastleKingside(WHITE) ? "YES" : "NO") << std::endl;
    std::cout << "White can castle queenside: " << (board.canCastleQueenside(WHITE) ? "YES" : "NO") << std::endl;
    
    // Check key squares for attacks
    std::cout << "\n=== SQUARE ATTACKS ===" << std::endl;
    std::cout << "E1 attacked by BLACK: " << (board.isSquareAttacked(E1, BLACK) ? "YES" : "NO") << std::endl;
    std::cout << "F1 attacked by BLACK: " << (board.isSquareAttacked(F1, BLACK) ? "YES" : "NO") << std::endl;
    std::cout << "G1 attacked by BLACK: " << (board.isSquareAttacked(G1, BLACK) ? "YES" : "NO") << std::endl;
    
    // Generate king moves
    MoveGenList<> kingMoves;
    generateKingMoves(board, kingMoves, WHITE);
    
    std::cout << "\n=== WHITE KING MOVES ===" << std::endl;
    std::cout << "Total king moves: " << kingMoves.size() << std::endl;
    
    bool kingsideCastling = false;
    bool queensideCastling = false;
    
    for (size_t i = 0; i < kingMoves.size(); ++i) {
        std::cout << "Move " << (i+1) << ": " << kingMoves[i].toString();
        
        if (kingMoves[i].isCastling()) {
            std::cout << " (CASTLING)";
            if (kingMoves[i].to() == G1) {
                kingsideCastling = true;
                std::cout << " - KINGSIDE";
            }
            if (kingMoves[i].to() == C1) {
                queensideCastling = true;
                std::cout << " - QUEENSIDE";
            }
        }
        std::cout << std::endl;
    }
    
    std::cout << "\n=== CASTLING ANALYSIS ===" << std::endl;
    std::cout << "Kingside castling move found: " << (kingsideCastling ? "YES" : "NO") << std::endl;
    std::cout << "Queenside castling move found: " << (queensideCastling ? "YES" : "NO") << std::endl;
    
    return 0;
}