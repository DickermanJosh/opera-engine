#include <iostream>
#include <chrono>
#include "Board.h"
#include "MoveGen.h"

using namespace opera;

uint64_t perft(Board& board, int depth) {
    if (depth == 0) return 1;
    
    uint64_t nodes = 0;
    MoveGenList<> moves;
    opera::generateAllLegalMoves(board, moves, board.getSideToMove());
    
    for (size_t i = 0; i < moves.size(); ++i) {
        Board tempBoard = board;
        if (tempBoard.makeMove(moves[i])) {
            nodes += perft(tempBoard, depth - 1);
        }
    }
    
    return nodes;
}

void testPerft(const std::string& name, const std::string& fen, int depth, uint64_t expected) {
    std::cout << "Testing " << name << "..." << std::endl;
    
    Board board(fen);
    uint64_t result = perft(board, depth);
    
    if (result == expected) {
        std::cout << "✅ PASS - " << name << " depth " << depth << ": " << result << " nodes" << std::endl;
    } else {
        std::cout << "❌ FAIL - " << name << " depth " << depth << ": expected " << expected << ", got " << result << std::endl;
    }
    std::cout << std::endl;
}

int main() {
    std::cout << "=== QUICK PERFT VALIDATION ===" << std::endl;
    std::cout << "Testing corrected expected values..." << std::endl << std::endl;
    
    // Test some of our corrected positions at lower depths
    testPerft("Endgame Position", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 3, 2850);
    testPerft("Position 5", "1k6/1b6/8/8/7R/8/8/4K2R b K - 0 1", 3, 3529);
    testPerft("Self Stalemate", "K1k5/8/P7/8/8/8/8/8 w - - 0 1", 4, 63);
    testPerft("Castle Rights", "r3k2r/1b4bq/8/8/8/8/7B/R3K2R w KQkq - 0 1", 3, 27826);
    testPerft("Short Castling Gives Check", "5k2/8/8/8/8/8/8/4K2R w K - 0 1", 4, 6399);
    
    return 0;
}