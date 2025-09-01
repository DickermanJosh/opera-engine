#include "Board.h"
#include "MoveGen.h"
#include <iostream>

using namespace opera;

int main() {
    Board board;
    board.setFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    
    std::cout << "Starting position board:" << std::endl;
    std::cout << board.toString() << std::endl;
    
    const Bitboard occupied = board.getOccupiedBitboard();
    const Bitboard whitePieces = board.getColorBitboard(WHITE);
    
    std::cout << "\nBitboard analysis:" << std::endl;
    std::cout << "Occupied bitboard: 0x" << std::hex << occupied << std::dec << std::endl;
    std::cout << "White pieces bitboard: 0x" << std::hex << whitePieces << std::dec << std::endl;
    
    // Test bishop attack generation for C1
    std::cout << "\nTesting C1 bishop attacks:" << std::endl;
    const Bitboard c1Attacks = board.getBishopAttacks(C1, occupied);
    std::cout << "C1 bishop raw attacks: 0x" << std::hex << c1Attacks << std::dec << std::endl;
    
    // Check specific squares
    std::cout << "Is B2 (" << static_cast<int>(B2) << ") set in attacks: " << ((c1Attacks & (1ULL << B2)) != 0) << std::endl;
    std::cout << "Is D2 (" << static_cast<int>(D2) << ") set in attacks: " << ((c1Attacks & (1ULL << D2)) != 0) << std::endl;
    std::cout << "Is B2 (" << static_cast<int>(B2) << ") set in white pieces: " << ((whitePieces & (1ULL << B2)) != 0) << std::endl;
    std::cout << "Is D2 (" << static_cast<int>(D2) << ") set in white pieces: " << ((whitePieces & (1ULL << D2)) != 0) << std::endl;
    
    // Test the actual piece values again
    std::cout << "\nPiece values:" << std::endl;
    std::cout << "Piece at B2: " << static_cast<int>(board.getPiece(B2)) << std::endl;
    std::cout << "Piece at D2: " << static_cast<int>(board.getPiece(D2)) << std::endl;
    
    return 0;
}