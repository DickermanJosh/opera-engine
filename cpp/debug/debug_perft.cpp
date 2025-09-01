#include <iostream>
#include <chrono>
#include "../include/Board.h"
#include "../include/MoveGen.h"

using namespace opera;

// Perft function - counts all possible moves to given depth
uint64_t perft(Board& board, int depth) {
    if (depth == 0) return 1;
    
    uint64_t nodes = 0;
    MoveGenList<> moves;
    opera::generateAllLegalMoves(board, moves, board.getSideToMove());
    
    for (size_t i = 0; i < moves.size(); ++i) {
        Board tempBoard = board;
        if (tempBoard.makeMove(moves[i])) {  // Only count legal moves
            nodes += perft(tempBoard, depth - 1);
        }
    }
    
    return nodes;
}

void testPerft(const std::string& name, const std::string& fen, int maxDepth = 3) {
    std::cout << "\n=== " << name << " ===" << std::endl;
    std::cout << "FEN: " << fen << std::endl;
    
    Board board(fen);
    board.print();
    
    for (int depth = 1; depth <= maxDepth; depth++) {
        auto start = std::chrono::high_resolution_clock::now();
        uint64_t nodes = perft(board, depth);
        auto end = std::chrono::high_resolution_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        
        std::cout << "Perft(" << depth << "): " << nodes << " nodes";
        if (duration.count() > 0) {
            std::cout << " (" << duration.count() << "ms, " 
                      << (nodes * 1000 / duration.count()) << " nps)";
        }
        std::cout << std::endl;
    }
}

int main() {
    std::cout << "=== OPERA ENGINE PERFT VALIDATION ===" << std::endl;
    std::cout << "Testing move generation accuracy with standard positions\n" << std::endl;
    
    // Starting position (should match known values)
    testPerft("Starting Position", 
              "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 4);
    
    // Kiwipete position - complex tactical position
    testPerft("Kiwipete Position", 
              "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 3);
    
    // Endgame position
    testPerft("Endgame Position", 
              "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 4);
    
    // Castling position
    testPerft("Castling Position", 
              "r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1", 4);
    
    // En passant position
    testPerft("En Passant Position", 
              "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1", 3);
    
    // Promotion position
    testPerft("Promotion Position", 
              "8/P7/8/8/8/8/8/K6k w - - 0 1", 3);
    
    std::cout << "\n=== PERFT VALIDATION COMPLETE ===" << std::endl;
    
    return 0;
}