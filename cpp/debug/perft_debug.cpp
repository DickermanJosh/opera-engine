/**
 * Perft Debug Tool - Detailed move-by-move analysis for failing tests
 *
 * This tool helps diagnose perft failures by showing exactly which moves
 * are being generated differently between expected and actual results.
 */

#include <iostream>
#include <iomanip>
#include <map>
#include <vector>
#include <string>
#include <functional>
#include "Board.h"
#include "MoveGen.h"

using namespace opera;

// Perft with move breakdown at root
void perftDivide(Board& board, int depth) {
    std::cout << "\n=== PERFT DIVIDE (Depth: " << depth << ") ===\n";
    std::cout << "FEN: " << board.toFEN() << "\n";
    std::cout << "Side to move: " << (board.getSideToMove() == WHITE ? "White" : "Black") << "\n\n";

    MoveGenList<256> moves;
    generateAllLegalMoves(board, moves, board.getSideToMove());

    std::cout << "Legal moves: " << moves.size() << "\n";
    std::cout << std::string(60, '-') << "\n";

    uint64_t totalNodes = 0;
    std::map<std::string, uint64_t> moveBreakdown;

    for (size_t i = 0; i < moves.size(); ++i) {
        Board tempBoard = board;
        const MoveGen& move = moves[i];

        if (!tempBoard.makeMove(move)) {
            std::cout << "WARNING: Move " << move.toString() << " failed to make!\n";
            continue;
        }

        // Count nodes for this move
        uint64_t nodes = 1;
        if (depth > 1) {
            std::function<uint64_t(Board&, int)> perft = [&perft](Board& b, int d) -> uint64_t {
                if (d == 0) return 1;

                uint64_t count = 0;
                MoveGenList<256> moves;
                generateAllLegalMoves(b, moves, b.getSideToMove());

                for (size_t j = 0; j < moves.size(); ++j) {
                    Board tempB = b;
                    if (tempB.makeMove(moves[j])) {
                        count += perft(tempB, d - 1);
                    }
                }
                return count;
            };

            nodes = perft(tempBoard, depth - 1);
        }

        std::string moveStr = move.toString();
        moveBreakdown[moveStr] = nodes;
        totalNodes += nodes;

        std::cout << std::setw(6) << moveStr << ": " << std::setw(12) << nodes;

        // Show move details
        std::cout << "  [";
        if (move.isCapture()) std::cout << "capture ";
        if (move.isPromotion()) std::cout << "promotion ";
        if (move.isCastling()) std::cout << "castling ";
        if (move.isEnPassant()) std::cout << "en-passant";
        std::cout << "]";

        std::cout << "\n";
    }

    std::cout << std::string(60, '-') << "\n";
    std::cout << "Total nodes: " << totalNodes << "\n";
}

int main(int argc, char* argv[]) {
    std::cout << "Opera Engine - Perft Debug Tool\n";
    std::cout << "================================\n\n";

    // Test cases that are failing
    struct TestCase {
        std::string name;
        std::string fen;
        int depth;
        uint64_t expected;
    };

    std::vector<TestCase> failingTests = {
        {
            "Endgame Position (Test 3)",
            "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
            5,
            681673
        },
        {
            "Illegal EP Move #1 (Test 6)",
            "3k4/3p4/8/K1P4r/8/8/8/8 b - - 0 1",
            5,
            186770
        },
        {
            "Illegal EP Move #2 (Test 7)",
            "8/8/4k3/8/2p5/8/B2P2K1/8 w - - 0 1",
            5,
            135530
        }
    };

    // Allow user to specify which test
    int testIndex = 0;
    int depth = 0;

    if (argc >= 2) {
        testIndex = std::atoi(argv[1]);
        if (testIndex < 0 || testIndex >= static_cast<int>(failingTests.size())) {
            std::cout << "Invalid test index. Available tests:\n";
            for (size_t i = 0; i < failingTests.size(); ++i) {
                std::cout << "  " << i << ": " << failingTests[i].name << "\n";
            }
            return 1;
        }
    }

    if (argc >= 3) {
        depth = std::atoi(argv[2]);
    }

    const auto& test = failingTests[testIndex];
    if (depth == 0) depth = test.depth;

    std::cout << "Analyzing: " << test.name << "\n";
    std::cout << "Expected: " << test.expected << " nodes\n";
    std::cout << "FEN: " << test.fen << "\n\n";

    try {
        Board board(test.fen);

        std::cout << "Current board state:\n";
        std::cout << board.toFEN() << "\n";

        // Run perft divide
        perftDivide(board, depth);

    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
