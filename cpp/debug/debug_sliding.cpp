#include "Board.h"
#include "MoveGen.h"
#include <iostream>

using namespace opera;

int main() {
    Board board;
    
    // Test FEN: "8/8/2P1P3/8/3B4/8/2P1P3/8 w - - 0 1"
    // Bishop on d4 with white pieces blocking some diagonals
    board.setFromFEN("8/8/2P1P3/8/3B4/8/2P1P3/8 w - - 0 1");
    
    std::cout << "Board position:" << std::endl;
    std::cout << board.toString() << std::endl;
    
    std::cout << "Bishop on D4: " << static_cast<int>(board.getPiece(D4)) << std::endl;
    std::cout << "Piece on C6 (square " << static_cast<int>(C6) << "): " << static_cast<int>(board.getPiece(C6)) << " (should be WHITE_PAWN = " << static_cast<int>(WHITE_PAWN) << ")" << std::endl;
    std::cout << "Piece on E6 (square " << static_cast<int>(E6) << "): " << static_cast<int>(board.getPiece(E6)) << " (should be WHITE_PAWN = " << static_cast<int>(WHITE_PAWN) << ")" << std::endl;
    std::cout << "Piece on C2 (square " << static_cast<int>(C2) << "): " << static_cast<int>(board.getPiece(C2)) << " (should be WHITE_PAWN = " << static_cast<int>(WHITE_PAWN) << ")" << std::endl;
    std::cout << "Piece on E2 (square " << static_cast<int>(E2) << "): " << static_cast<int>(board.getPiece(E2)) << " (should be WHITE_PAWN = " << static_cast<int>(WHITE_PAWN) << ")" << std::endl;
    
    const Bitboard occupied = board.getOccupiedBitboard();
    const Bitboard ownPieces = board.getColorBitboard(WHITE);
    
    // Get raw attack bitboard
    const Bitboard rawAttacks = board.getBishopAttacks(D4, occupied);
    
    std::cout << std::endl << "Manual bit checks:" << std::endl;
    std::cout << "Is C6 (" << static_cast<int>(C6) << ") set in ownPieces: " << ((ownPieces & (1ULL << C6)) != 0) << std::endl;
    std::cout << "Is E6 (" << static_cast<int>(E6) << ") set in ownPieces: " << ((ownPieces & (1ULL << E6)) != 0) << std::endl;
    std::cout << "Is C2 (" << static_cast<int>(C2) << ") set in ownPieces: " << ((ownPieces & (1ULL << C2)) != 0) << std::endl;
    std::cout << "Is E2 (" << static_cast<int>(E2) << ") set in ownPieces: " << ((ownPieces & (1ULL << E2)) != 0) << std::endl;
    
    std::cout << "Is C6 (" << static_cast<int>(C6) << ") set in rawAttacks: " << ((rawAttacks & (1ULL << C6)) != 0) << std::endl;
    std::cout << "Is E6 (" << static_cast<int>(E6) << ") set in rawAttacks: " << ((rawAttacks & (1ULL << E6)) != 0) << std::endl;
    std::cout << "Is C2 (" << static_cast<int>(C2) << ") set in rawAttacks: " << ((rawAttacks & (1ULL << C2)) != 0) << std::endl;
    std::cout << "Is E2 (" << static_cast<int>(E2) << ") set in rawAttacks: " << ((rawAttacks & (1ULL << E2)) != 0) << std::endl;
    
    std::cout << std::endl << "Bitboards (hex):" << std::endl;
    std::cout << "Occupied: 0x" << std::hex << occupied << std::dec << std::endl;
    std::cout << "Own pieces (WHITE): 0x" << std::hex << ownPieces << std::dec << std::endl;
    std::cout << "Raw bishop attacks: 0x" << std::hex << rawAttacks << std::dec << std::endl;
    
    // Apply mask to exclude own pieces
    const Bitboard filteredAttacks = rawAttacks & ~ownPieces;
    std::cout << "Filtered attacks (raw & ~own): 0x" << std::hex << filteredAttacks << std::dec << std::endl;
    
    // Count bits in each
    std::cout << std::endl << "Bit counts:" << std::endl;
    std::cout << "Raw attacks bit count: " << __builtin_popcountll(rawAttacks) << std::endl;
    std::cout << "Own pieces bit count: " << __builtin_popcountll(ownPieces) << std::endl;
    std::cout << "Filtered attacks bit count: " << __builtin_popcountll(filteredAttacks) << std::endl;
    
    std::cout << std::endl << "Move generation test:" << std::endl;
    MoveGenList<> moves;
    generateBishopMoves(board, moves, WHITE);
    
    std::cout << "Generated " << moves.size() << " bishop moves (expected 4):" << std::endl;
    for (size_t i = 0; i < moves.size(); ++i) {
        std::cout << "  " << i << ": " << moves[i].toString() 
                  << " (from=" << static_cast<int>(moves[i].from()) 
                  << ", to=" << static_cast<int>(moves[i].to())
                  << ")" << std::endl;
    }
    
    return 0;
}