#include <iostream>
#include <chrono>
#include "Board.h"
#include "MoveGen.h"

using namespace opera;

// Perft function
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

void testPerft(const std::string& name, const std::string& fen, int maxDepth = 4) {
    std::cout << "=== " << name << " ===" << std::endl;
    std::cout << "FEN: " << fen << std::endl;
    
    Board board(fen);
    
    for (int depth = 1; depth <= maxDepth; depth++) {
        uint64_t result = perft(board, depth);
        std::cout << "Perft(" << depth << "): " << result << std::endl;
    }
    std::cout << std::endl;
}

int main() {
    std::cout << "=== FAILING PERFT POSITIONS ANALYSIS ===" << std::endl;
    std::cout << "Getting actual values from our engine\n" << std::endl;
    
    // These are failing - let's get actual values
    testPerft("EndgamePosition", 
              "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 5);
              
    testPerft("Position5", 
              "1k6/1b6/8/8/7R/8/8/4K2R b K - 0 1", 4);
              
    testPerft("IllegalEpMove1", 
              "3k4/3p4/8/K1P4r/8/8/8/8 b - - 0 1", 5);
              
    testPerft("IllegalEpMove2", 
              "8/8/4k3/8/2p5/8/B2P2K1/8 w - - 0 1", 5);
              
    testPerft("EpCaptureChecksOpponent", 
              "8/8/1k6/2b5/2pP4/8/5K2/8 b - d3 0 1", 5);
              
    testPerft("ShortCastlingGivesCheck", 
              "5k2/8/8/8/8/8/8/4K2R w K - 0 1", 5);
              
    testPerft("CastleRights", 
              "r3k2r/1b4bq/8/8/8/8/7B/R3K2R w KQkq - 0 1", 3);
              
    testPerft("CastlingPrevented", 
              "r3k2r/8/3Q4/8/8/5q2/8/R3K2R b KQkq - 0 1", 3);
              
    testPerft("PromoteOutOfCheck", 
              "2K2r2/4P3/8/8/8/8/8/3k4 w - - 0 1", 5);
              
    testPerft("DiscoveredCheck", 
              "8/8/1P2K3/8/2n5/1q6/8/5k2 b - - 0 1", 4);
              
    testPerft("SelfStalemate", 
              "K1k5/8/P7/8/8/8/8/8 w - - 0 1", 5);
              
    testPerft("LongCastlingGivesCheck", 
              "3k4/8/8/8/8/8/8/R3K3 w Q - 0 1", 5);
              
    testPerft("StalemateCheckmate1", 
              "8/k1P5/8/1K6/8/8/8/8 w - - 0 1", 6);
    
    return 0;
}