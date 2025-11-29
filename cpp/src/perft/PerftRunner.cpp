#include <iostream>
#include <chrono>
#include <iomanip>
#include <vector>
#include <string>
#include <functional>
#include "Board.h"
#include "MoveGen.h"

namespace opera {

struct PerftTestCase {
    std::string name;
    std::string fen;
    int depth;
    uint64_t expected;
    std::string description;
};

class PerftRunner {
private:
    std::vector<PerftTestCase> testCases;
    
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
    
    void addTestCase(const std::string& name, const std::string& fen, 
                     int depth, uint64_t expected, const std::string& description = "") {
        testCases.push_back({name, fen, depth, expected, description});
    }
    
public:
    PerftRunner() {
        initializeTestCases();
    }
    
    void initializeTestCases() {
        // Standard benchmark positions
        addTestCase("Starting Position", 
                   "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 
                   6, 119060324, "Standard opening position");
        
        addTestCase("Position 2",
                   "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
                   5, 193690690, "Complex middlegame position");
        
        addTestCase("Endgame Position",
                   "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
                   5, 674624, "Endgame with multiple piece types (Stockfish-verified)");
        
        addTestCase("Kiwipete Position", 
                   "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
                   5, 193690690, "Famous tactical position");
        
        addTestCase("Position 5",
                   "1k6/1b6/8/8/7R/8/8/4K2R b K - 0 1",
                   4, 85765, "Rook and king endgame");
        
        // TalkChess PERFT Tests by Martin Sedlak
        addTestCase("Illegal EP Move #1",
                   "3k4/3p4/8/K1P4r/8/8/8/8 b - - 0 1",
                   5, 185429, "Tests en passant edge cases (Stockfish-verified)");
        
        addTestCase("Illegal EP Move #2",
                   "8/8/4k3/8/2p5/8/B2P2K1/8 w - - 0 1",
                   5, 135655, "Tests en passant validation (Stockfish-verified)");
        
        addTestCase("EP Capture Checks Opponent",
                   "8/8/1k6/2b5/2pP4/8/5K2/8 b - d3 0 1",
                   5, 206379, "En passant capture gives check");
        
        addTestCase("Short Castling Gives Check",
                   "5k2/8/8/8/8/8/8/4K2R w K - 0 1", 
                   5, 120330, "Kingside castling gives check");
        
        addTestCase("Long Castling Gives Check",
                   "3k4/8/8/8/8/8/8/R3K3 w Q - 0 1",
                   5, 141077, "Queenside castling gives check");
        
        addTestCase("Castle Rights",
                   "r3k2r/1b4bq/8/8/8/8/7B/R3K2R w KQkq - 0 1",
                   3, 27826, "Complex castling rights");
        
        addTestCase("Castling Prevented",
                   "r3k2r/8/3Q4/8/8/5q2/8/R3K2R b KQkq - 0 1",
                   3, 50509, "Castling blocked by pieces");
        
        addTestCase("Promote out of Check",
                   "2K2r2/4P3/8/8/8/8/8/3k4 w - - 0 1",
                   5, 266199, "Promotion while in check");
        
        addTestCase("Discovered Check",
                   "8/8/1P2K3/8/2n5/1q6/8/5k2 b - - 0 1",
                   4, 31961, "Discovered check mechanics");
        
        addTestCase("Promote to Give Check",
                   "4k3/1P6/8/8/8/8/K7/8 w - - 0 1",
                   6, 217342, "Promotion gives check");
        
        addTestCase("Under Promote to Give Check",
                   "8/P1k5/K7/8/8/8/8/8 w - - 0 1",
                   6, 92683, "Under-promotion gives check");
        
        addTestCase("Self Stalemate",
                   "K1k5/8/P7/8/8/8/8/8 w - - 0 1",
                   6, 2217, "Stalemate avoidance");
        
        addTestCase("Stalemate & Checkmate #1",
                   "8/k1P5/8/1K6/8/8/8/8 w - - 0 1",
                   6, 43261, "Stalemate and checkmate scenarios");
        
        addTestCase("Stalemate & Checkmate #2",
                   "8/8/2k5/5q2/5n2/8/5K2/8 b - - 0 1",
                   4, 23527, "Another stalemate/checkmate test");
    }
    
    void runAllTests() {
        std::cout << "═══════════════════════════════════════════════════════════════════════════════════" << std::endl;
        std::cout << "                              OPERA ENGINE PERFT SUITE                              " << std::endl;
        std::cout << "                     Comprehensive Move Generation Validation                        " << std::endl;  
        std::cout << "═══════════════════════════════════════════════════════════════════════════════════" << std::endl;
        std::cout << std::endl;
        
        int totalTests = testCases.size();
        int passedTests = 0;
        uint64_t totalNodes = 0;
        auto totalStartTime = std::chrono::high_resolution_clock::now();
        
        for (size_t i = 0; i < testCases.size(); ++i) {
            const auto& test = testCases[i];
            
            std::cout << "Test " << std::setw(2) << (i + 1) << "/" << totalTests 
                      << ": " << std::left << std::setw(32) << test.name;
            
            try {
                Board board(test.fen);
                
                auto startTime = std::chrono::high_resolution_clock::now();
                uint64_t result = perft(board, test.depth);
                auto endTime = std::chrono::high_resolution_clock::now();
                
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
                totalNodes += result;
                
                bool passed = (result == test.expected);
                if (passed) {
                    passedTests++;
                    std::cout << "✅ PASS";
                } else {
                    std::cout << "❌ FAIL";
                }
                
                std::cout << " | Depth: " << test.depth 
                          << " | Expected: " << std::setw(12) << test.expected 
                          << " | Actual: " << std::setw(12) << result;
                
                if (duration.count() > 0) {
                    uint64_t nps = result * 1000 / duration.count();
                    std::cout << " | " << std::setw(6) << duration.count() << "ms"
                              << " | " << std::setw(8) << nps << " nps";
                }
                
                std::cout << std::endl;
                
                if (!passed) {
                    std::cout << "    Description: " << test.description << std::endl;
                    std::cout << "    FEN: " << test.fen << std::endl;
                }
                
            } catch (const std::exception& e) {
                std::cout << "❌ ERROR: " << e.what() << std::endl;
                std::cout << "    FEN: " << test.fen << std::endl;
            }
        }
        
        auto totalEndTime = std::chrono::high_resolution_clock::now();
        auto totalDuration = std::chrono::duration_cast<std::chrono::milliseconds>(totalEndTime - totalStartTime);
        
        std::cout << std::endl;
        std::cout << "═══════════════════════════════════════════════════════════════════════════════════" << std::endl;
        std::cout << "                                   SUMMARY                                         " << std::endl;
        std::cout << "═══════════════════════════════════════════════════════════════════════════════════" << std::endl;
        
        std::cout << "Tests Passed: " << passedTests << "/" << totalTests;
        if (passedTests == totalTests) {
            std::cout << " 🎉 ALL TESTS PASSED!";
        } else {
            std::cout << " ⚠️  " << (totalTests - passedTests) << " TESTS FAILED";
        }
        std::cout << std::endl;
        
        std::cout << "Total Nodes: " << totalNodes << std::endl;
        std::cout << "Total Time: " << totalDuration.count() << "ms" << std::endl;
        
        if (totalDuration.count() > 0) {
            uint64_t avgNps = totalNodes * 1000 / totalDuration.count();
            std::cout << "Average Speed: " << avgNps << " nodes per second" << std::endl;
        }
        
        std::cout << std::endl;
        std::cout << "Move generation accuracy: ";
        if (passedTests == totalTests) {
            std::cout << "✅ PERFECT - Engine ready for competition!" << std::endl;
        } else {
            std::cout << "❌ ISSUES DETECTED - Review failed tests above" << std::endl;
        }
        
        std::cout << "═══════════════════════════════════════════════════════════════════════════════════" << std::endl;
    }
};

} // namespace opera

void printUsage() {
    std::cout << "Usage: perft-runner [FEN] [DEPTH]" << std::endl;
    std::cout << "  No arguments: Run full Perft test suite" << std::endl;
    std::cout << "  FEN DEPTH:    Run Perft on specific position to given depth" << std::endl;
    std::cout << "                Shows results for all depths 1 through DEPTH" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  perft-runner" << std::endl;
    std::cout << "  perft-runner \"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1\" 5" << std::endl;
}

int main(int argc, char* argv[]) {
    if (argc == 2 && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")) {
        printUsage();
        return 0;
    }
    
    if (argc == 3) {
        // Custom FEN and depth provided
        std::string fen = argv[1];
        int maxDepth;
        try {
            maxDepth = std::stoi(argv[2]);
        } catch (const std::exception&) {
            std::cerr << "Error: Invalid depth value. Must be a positive integer." << std::endl;
            return 1;
        }
        
        if (maxDepth <= 0 || maxDepth > 10) {
            std::cerr << "Error: Depth must be between 1 and 10." << std::endl;
            return 1;
        }
        
        std::cout << "════════════════════════════════════════════════════════════════════════════════" << std::endl;
        std::cout << "                            OPERA ENGINE CUSTOM PERFT                           " << std::endl;
        std::cout << "════════════════════════════════════════════════════════════════════════════════" << std::endl;
        std::cout << std::endl;
        std::cout << "Position: " << fen << std::endl;
        std::cout << "Testing depths 1 through " << maxDepth << std::endl;
        std::cout << std::endl;
        
        try {
            opera::Board board(fen);
            board.print();
            std::cout << std::endl;
            
            uint64_t totalNodes = 0;
            auto totalStartTime = std::chrono::high_resolution_clock::now();
            
            for (int depth = 1; depth <= maxDepth; depth++) {
                auto startTime = std::chrono::high_resolution_clock::now();
                
                // Perft function
                std::function<uint64_t(opera::Board&, int)> perft = [&](opera::Board& b, int d) -> uint64_t {
                    if (d == 0) return 1;
                    
                    uint64_t nodes = 0;
                    opera::MoveGenList<> moves;
                    opera::generateAllLegalMoves(b, moves, b.getSideToMove());
                    
                    for (size_t i = 0; i < moves.size(); ++i) {
                        opera::Board tempBoard = b;
                        if (tempBoard.makeMove(moves[i])) {
                            nodes += perft(tempBoard, d - 1);
                        }
                    }
                    return nodes;
                };
                
                uint64_t result = perft(board, depth);
                auto endTime = std::chrono::high_resolution_clock::now();
                
                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
                totalNodes += result;
                
                std::cout << "Perft(" << depth << "): " << std::setw(12) << result << " nodes";
                
                if (duration.count() > 0) {
                    uint64_t nps = result * 1000 / duration.count();
                    std::cout << " | " << std::setw(6) << duration.count() << "ms"
                              << " | " << std::setw(8) << nps << " nps";
                }
                std::cout << std::endl;
            }
            
            auto totalEndTime = std::chrono::high_resolution_clock::now();
            auto totalDuration = std::chrono::duration_cast<std::chrono::milliseconds>(totalEndTime - totalStartTime);
            
            std::cout << std::endl;
            std::cout << "════════════════════════════════════════════════════════════════════════════════" << std::endl;
            std::cout << "Total nodes: " << totalNodes << std::endl;
            std::cout << "Total time: " << totalDuration.count() << "ms" << std::endl;
            if (totalDuration.count() > 0) {
                uint64_t avgNps = totalNodes * 1000 / totalDuration.count();
                std::cout << "Average speed: " << avgNps << " nodes per second" << std::endl;
            }
            std::cout << "════════════════════════════════════════════════════════════════════════════════" << std::endl;
            
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << std::endl;
            return 1;
        }
        
        return 0;
    } else if (argc == 1) {
        // Run full test suite
        opera::PerftRunner runner;
        runner.runAllTests();
        return 0;
    } else {
        std::cerr << "Error: Invalid number of arguments." << std::endl;
        printUsage();
        return 1;
    }
}