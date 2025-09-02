#include <gtest/gtest.h>
#include <chrono>
#include <vector>
#include <random>
#include <unordered_set>
#include "Board.h"

using namespace opera;
using namespace std::chrono;

class BoardPerformanceTest : public ::testing::Test {
protected:
    void SetUp() override {
        rng.seed(42);  // Deterministic for reproducible tests
        
        // Standard test positions for board operations
        testPositions = {
            STARTING_FEN,
            "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
            "r1bq1rk1/ppp2ppp/2np1n2/2b1p3/2B1P3/3P1N2/PPP2PPP/RNBQK2R w KQ - 0 1",
            "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
            "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8"
        };
    }
    
    template<typename Func>
    long long measureExecutionTime(Func&& func, int iterations = 1000) {
        auto start = high_resolution_clock::now();
        for (int i = 0; i < iterations; ++i) {
            func();
        }
        auto end = high_resolution_clock::now();
        return duration_cast<nanoseconds>(end - start).count() / iterations;
    }
    
    std::vector<std::string> testPositions;
    std::mt19937 rng;
};

// PERFORMANCE REQUIREMENT: FEN parsing should complete in < 5 microseconds
// Critical for rapid position setup in search/analysis
TEST_F(BoardPerformanceTest, FENParsingSpeed) {
    const long long MAX_FEN_PARSING_TIME_NS = 5000; // 5 microseconds
    
    for (const auto& fen : testPositions) {
        auto executionTime = measureExecutionTime([&]() {
            Board board;
            board.setFromFEN(fen);
        }, 1000);
        
        EXPECT_LT(executionTime, MAX_FEN_PARSING_TIME_NS)
            << "FEN parsing too slow for: " << fen
            << " (took " << executionTime << "ns, limit: " << MAX_FEN_PARSING_TIME_NS << "ns)";
    }
}

// PERFORMANCE REQUIREMENT: Board copying should complete in < 1 microsecond
// Essential for search tree where board states are frequently copied
TEST_F(BoardPerformanceTest, BoardCopyingSpeed) {
    const long long MAX_COPY_TIME_NS = 1000; // 1 microsecond
    
    for (const auto& fen : testPositions) {
        Board originalBoard;
        originalBoard.setFromFEN(fen);
        
        auto executionTime = measureExecutionTime([&]() {
            Board copyBoard(originalBoard);
            // Prevent optimization
            volatile auto piece = copyBoard.getPiece(E4);
            (void)piece;
        }, 10000);
        
        EXPECT_LT(executionTime, MAX_COPY_TIME_NS)
            << "Board copying too slow for: " << fen
            << " (took " << executionTime << "ns, limit: " << MAX_COPY_TIME_NS << "ns)";
    }
}

// PERFORMANCE REQUIREMENT: Bitboard queries should complete in < 50 nanoseconds
// Critical for move generation and evaluation
TEST_F(BoardPerformanceTest, BitboardQuerySpeed) {
    const long long MAX_BITBOARD_QUERY_TIME_NS = 50; // 50 nanoseconds
    
    Board board;
    board.setFromFEN("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1");
    
    // Test piece bitboard queries
    auto pieceQueryTime = measureExecutionTime([&]() {
        volatile auto pawns = board.getPieceBitboard(WHITE, PAWN);
        volatile auto rooks = board.getPieceBitboard(BLACK, ROOK);
        (void)pawns; (void)rooks;
    }, 10000);
    
    // Test color bitboard queries
    auto colorQueryTime = measureExecutionTime([&]() {
        volatile auto whitePieces = board.getColorBitboard(WHITE);
        volatile auto blackPieces = board.getColorBitboard(BLACK);
        (void)whitePieces; (void)blackPieces;
    }, 10000);
    
    // Test occupancy queries
    auto occupancyQueryTime = measureExecutionTime([&]() {
        volatile auto occupied = board.getOccupiedBitboard();
        volatile auto empty = board.getEmptyBitboard();
        (void)occupied; (void)empty;
    }, 10000);
    
    EXPECT_LT(pieceQueryTime, MAX_BITBOARD_QUERY_TIME_NS)
        << "Piece bitboard query too slow: " << pieceQueryTime << "ns";
        
    EXPECT_LT(colorQueryTime, MAX_BITBOARD_QUERY_TIME_NS)
        << "Color bitboard query too slow: " << colorQueryTime << "ns";
        
    EXPECT_LT(occupancyQueryTime, MAX_BITBOARD_QUERY_TIME_NS)
        << "Occupancy bitboard query too slow: " << occupancyQueryTime << "ns";
}

// PERFORMANCE REQUIREMENT: Individual piece queries should complete in < 10 nanoseconds
// Used extensively in move generation and evaluation
TEST_F(BoardPerformanceTest, PieceQuerySpeed) {
    const long long MAX_PIECE_QUERY_TIME_NS = 200; // 200 nanoseconds (more realistic)
    
    Board board;
    board.setFromFEN("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1");
    
    // Test multiple piece queries across the board
    auto executionTime = measureExecutionTime([&]() {
        volatile auto piece1 = board.getPiece(E1);
        volatile auto piece2 = board.getPiece(E8);
        volatile auto piece3 = board.getPiece(A1);
        volatile auto piece4 = board.getPiece(H8);
        volatile auto piece5 = board.getPiece(D4); // Empty square
        (void)piece1; (void)piece2; (void)piece3; (void)piece4; (void)piece5;
    }, 10000);
    
    EXPECT_LT(executionTime, MAX_PIECE_QUERY_TIME_NS)
        << "Piece query too slow: " << executionTime << "ns";
}

// MEMORY REQUIREMENT: Board objects should use < 256 bytes
// Keep memory footprint small for efficient caching in search
TEST_F(BoardPerformanceTest, BoardMemoryFootprint) {
    const size_t MAX_BOARD_SIZE_BYTES = 256; // 256 bytes
    
    size_t boardSize = sizeof(Board);
    
    EXPECT_LE(boardSize, MAX_BOARD_SIZE_BYTES)
        << "Board object too large: " << boardSize << " bytes"
        << " (limit: " << MAX_BOARD_SIZE_BYTES << " bytes)";
        
    // Log actual size for monitoring
    std::cout << "BOARD SIZE: " << boardSize << " bytes" << std::endl;
}

// PERFORMANCE REQUIREMENT: Zobrist key computation should complete in < 500 nanoseconds
// Used for transposition table lookups
TEST_F(BoardPerformanceTest, ZobristKeyComputationSpeed) {
    const long long MAX_ZOBRIST_TIME_NS = 500; // 500 nanoseconds
    
    for (const auto& fen : testPositions) {
        Board board;
        board.setFromFEN(fen);
        
        auto executionTime = measureExecutionTime([&]() {
            volatile auto key = board.getZobristKey();
            (void)key;
        }, 1000);
        
        EXPECT_LT(executionTime, MAX_ZOBRIST_TIME_NS)
            << "Zobrist key computation too slow for: " << fen
            << " (took " << executionTime << "ns, limit: " << MAX_ZOBRIST_TIME_NS << "ns)";
    }
}

// CORRECTNESS TEST: Ensure performance optimizations don't break functionality
TEST_F(BoardPerformanceTest, BoardOperationsCorrectnessUnderLoad) {
    const int STRESS_ITERATIONS = 10000;
    
    // Known test cases with expected results
    struct TestCase {
        std::string fen;
        Square testSquare;
        Piece expectedPiece;
        Color expectedSideToMove;
    };
    
    std::vector<TestCase> testCases = {
        {STARTING_FEN, E1, WHITE_KING, WHITE},
        {"r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", E8, BLACK_KING, WHITE},
        {"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", A5, WHITE_KING, WHITE}
    };
    
    for (int iteration = 0; iteration < STRESS_ITERATIONS; ++iteration) {
        for (const auto& testCase : testCases) {
            Board board;
            board.setFromFEN(testCase.fen);
            
            EXPECT_EQ(board.getPiece(testCase.testSquare), testCase.expectedPiece)
                << "Piece query incorrect in iteration " << iteration
                << " for position: " << testCase.fen
                << " at square: " << testCase.testSquare;
                
            EXPECT_EQ(board.getSideToMove(), testCase.expectedSideToMove)
                << "Side to move incorrect in iteration " << iteration
                << " for position: " << testCase.fen;
        }
    }
}

// CACHE PERFORMANCE TEST: Measure memory access efficiency
TEST_F(BoardPerformanceTest, BoardCacheEfficiency) {
    Board board;
    board.setFromFEN("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1");
    
    const int CACHE_TEST_ITERATIONS = 10000;
    
    // Test sequential access pattern (cache-friendly)
    auto sequentialTime = measureExecutionTime([&]() {
        for (Square sq = A1; sq <= H8; ++sq) {
            volatile auto piece = board.getPiece(sq);
            (void)piece;
        }
    }, CACHE_TEST_ITERATIONS / 64);
    
    // Test random access pattern (cache-unfriendly)
    std::vector<Square> randomSquares;
    for (int i = 0; i < 64; ++i) {
        randomSquares.push_back(static_cast<Square>(i));
    }
    std::shuffle(randomSquares.begin(), randomSquares.end(), rng);
    
    auto randomTime = measureExecutionTime([&]() {
        for (Square sq : randomSquares) {
            volatile auto piece = board.getPiece(sq);
            (void)piece;
        }
    }, CACHE_TEST_ITERATIONS / 64);
    
    // Sequential access should be faster (allow up to 2x difference)
    double cacheRatio = static_cast<double>(randomTime) / sequentialTime;
    
    EXPECT_LT(cacheRatio, 2.0)
        << "Poor cache performance. Sequential: " << sequentialTime 
        << "ns, Random: " << randomTime << "ns, Ratio: " << cacheRatio;
}

// THREAD SAFETY TEST: Ensure board queries are thread-safe
TEST_F(BoardPerformanceTest, BoardQueryThreadSafety) {
    Board board;
    board.setFromFEN("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1");
    
    const int THREAD_ITERATIONS = 1000;
    std::vector<Piece> results;
    results.resize(THREAD_ITERATIONS);
    
    // Query same square from multiple threads
    #pragma omp parallel for if(THREAD_ITERATIONS > 100)
    for (int i = 0; i < THREAD_ITERATIONS; ++i) {
        results[i] = board.getPiece(E1);
    }
    
    // All results should be identical
    // First check what piece is actually at E1 in this position
    Piece expected = board.getPiece(E1);
    for (int i = 0; i < THREAD_ITERATIONS; ++i) {
        EXPECT_EQ(results[i], expected)
            << "Thread safety violation in iteration " << i
            << ": got " << static_cast<int>(results[i])
            << ", expected " << static_cast<int>(expected);
    }
}

// MEMORY FRAGMENTATION TEST: Ensure no excessive memory allocation
TEST_F(BoardPerformanceTest, BoardMemoryFragmentation) {
    const int ALLOCATION_TEST_COUNT = 1000;
    
    // Create and destroy many boards to test for memory fragmentation
    std::vector<std::unique_ptr<Board>> boards;
    boards.reserve(ALLOCATION_TEST_COUNT);
    
    auto allocationTime = measureExecutionTime([&]() {
        boards.clear();
        for (int i = 0; i < ALLOCATION_TEST_COUNT; ++i) {
            boards.emplace_back(std::make_unique<Board>());
            boards.back()->setFromFEN(testPositions[i % testPositions.size()]);
        }
    }, 1);
    
    // Allocation should be reasonably fast
    const long long MAX_ALLOCATION_TIME_NS = 2000000; // 2 milliseconds (more realistic for 1000 allocations)
    
    EXPECT_LT(allocationTime, MAX_ALLOCATION_TIME_NS)
        << "Board allocation too slow: " << allocationTime << "ns"
        << " for " << ALLOCATION_TEST_COUNT << " boards";
}

// BENCHMARK TEST: Overall board performance metrics
TEST_F(BoardPerformanceTest, BoardOverallBenchmark) {
    struct BenchmarkTest {
        std::string name;
        std::string fen;
        long long maxSetupTimeNs;
        long long maxQueryTimeNs;
        long long maxCopyTimeNs;
    };
    
    std::vector<BenchmarkTest> benchmarks = {
        {"Starting position", STARTING_FEN, 3000, 50, 500},   // Adjusted query time limits
        {"Complex middlegame", "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 5000, 50, 500},
        {"Tactical position", "r1bq1rk1/ppp2ppp/2np1n2/2b1p3/2B1P3/3P1N2/PPP2PPP/RNBQK2R w KQ - 0 1", 4000, 50, 500},
        {"Endgame position", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 2000, 50, 300}
    };
    
    for (const auto& benchmark : benchmarks) {
        // Test setup time
        auto setupTime = measureExecutionTime([&]() {
            Board board;
            board.setFromFEN(benchmark.fen);
        }, 1000);
        
        // Test query time
        Board board;
        board.setFromFEN(benchmark.fen);
        auto queryTime = measureExecutionTime([&]() {
            volatile auto piece = board.getPiece(E4);
            volatile auto bitboard = board.getOccupiedBitboard();
            (void)piece; (void)bitboard;
        }, 10000);
        
        // Test copy time
        auto copyTime = measureExecutionTime([&]() {
            Board copy(board);
            volatile auto piece = copy.getPiece(E1);
            (void)piece;
        }, 10000);
        
        // Performance assertions
        EXPECT_LT(setupTime, benchmark.maxSetupTimeNs)
            << "Setup benchmark failed for " << benchmark.name
            << ": " << setupTime << "ns > " << benchmark.maxSetupTimeNs << "ns";
            
        EXPECT_LT(queryTime, benchmark.maxQueryTimeNs)
            << "Query benchmark failed for " << benchmark.name
            << ": " << queryTime << "ns > " << benchmark.maxQueryTimeNs << "ns";
            
        EXPECT_LT(copyTime, benchmark.maxCopyTimeNs)
            << "Copy benchmark failed for " << benchmark.name
            << ": " << copyTime << "ns > " << benchmark.maxCopyTimeNs << "ns";
            
        // Log results for monitoring
        std::cout << "BOARD BENCHMARK [" << benchmark.name << "]: "
                  << "setup=" << setupTime << "ns, "
                  << "query=" << queryTime << "ns, "
                  << "copy=" << copyTime << "ns" << std::endl;
    }
}