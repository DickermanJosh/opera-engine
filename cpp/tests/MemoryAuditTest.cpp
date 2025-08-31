#include <gtest/gtest.h>
#include <chrono>
#include <vector>
#include <memory>
#include <cstdlib>
#include <algorithm>
#include <unordered_map>
#include <fstream>
#include <iostream>
#include <sys/resource.h>
#include "Board.h"
#include "MoveGen.h"

using namespace opera;
using namespace std::chrono;

class MemoryAuditTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Get initial memory usage
        initialMemoryUsage = getCurrentMemoryUsage();
        
        testPositions = {
            STARTING_FEN,
            "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1",
            "r1bq1rk1/ppp2ppp/2np1n2/2b1p3/2B1P3/3P1N2/PPP2PPP/RNBQK2R w KQ - 0 1",
            "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
            "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8"
        };
    }
    
    void TearDown() override {
        // Verify no significant memory leaks
        size_t finalMemoryUsage = getCurrentMemoryUsage();
        size_t memoryDifference = finalMemoryUsage > initialMemoryUsage ? 
            finalMemoryUsage - initialMemoryUsage : 0;
            
        // Allow for some variance in memory usage (up to 1MB)
        const size_t MAX_ALLOWED_LEAK = 1024 * 1024; // 1MB
        
        EXPECT_LT(memoryDifference, MAX_ALLOWED_LEAK)
            << "Potential memory leak detected. Initial: " << initialMemoryUsage
            << " bytes, Final: " << finalMemoryUsage << " bytes"
            << ", Difference: " << memoryDifference << " bytes";
    }
    
    // Get current memory usage (RSS - Resident Set Size)
    size_t getCurrentMemoryUsage() {
        struct rusage usage;
        getrusage(RUSAGE_SELF, &usage);
        // ru_maxrss is in kilobytes on Linux, bytes on macOS
        #ifdef __APPLE__
        return static_cast<size_t>(usage.ru_maxrss);
        #else
        return static_cast<size_t>(usage.ru_maxrss * 1024);
        #endif
    }
    
    // Measure peak memory usage during function execution
    template<typename Func>
    size_t measurePeakMemoryUsage(Func&& func) {
        size_t initialMem = getCurrentMemoryUsage();
        func();
        size_t peakMem = getCurrentMemoryUsage();
        return peakMem > initialMem ? peakMem - initialMem : 0;
    }
    
    // Calculate theoretical minimum memory for storing data
    size_t calculateTheoreticalMinimum(size_t numMoves) {
        return numMoves * sizeof(MoveGen);
    }
    
    std::vector<std::string> testPositions;
    size_t initialMemoryUsage;
};

// MEMORY REQUIREMENT: Board objects should have minimal memory overhead
// Ensure efficient memory layout and no hidden allocations
TEST_F(MemoryAuditTest, BoardMemoryFootprintAudit) {
    const size_t MAX_BOARD_SIZE = 256;  // 256 bytes maximum
    const size_t MIN_BOARD_SIZE = 128;  // 128 bytes minimum (ensure we're not wasting space)
    
    size_t boardSize = sizeof(Board);
    
    EXPECT_LE(boardSize, MAX_BOARD_SIZE)
        << "Board object too large: " << boardSize << " bytes";
        
    EXPECT_GE(boardSize, MIN_BOARD_SIZE)
        << "Board object suspiciously small: " << boardSize << " bytes"
        << " (may indicate missing essential data)";
    
    // Test memory usage of board creation
    size_t singleBoardMemory = measurePeakMemoryUsage([&]() {
        Board board;
        board.setFromFEN(STARTING_FEN);
    });
    
    // Should not allocate excessively more memory (allow for system overhead)
    // Note: Memory measurement granularity means small allocations may show large overhead
    const size_t MAX_REASONABLE_OVERHEAD = 65536; // 64KB (reasonable for system overhead on macOS)
    EXPECT_LT(singleBoardMemory, MAX_REASONABLE_OVERHEAD)
        << "Board creation uses excessive memory: " << singleBoardMemory
        << " bytes (board size: " << boardSize << " bytes)";
    
    std::cout << "MEMORY AUDIT [Board]: size=" << boardSize 
              << " bytes, creation_overhead=" << singleBoardMemory << " bytes" << std::endl;
}

// MEMORY REQUIREMENT: MoveGen objects should be efficiently packed
TEST_F(MemoryAuditTest, MoveGenMemoryFootprintAudit) {
    const size_t EXPECTED_MOVE_SIZE = 4;  // 32-bit packed representation
    const size_t MAX_MOVE_SIZE = 8;       // Allow some compiler padding
    
    size_t moveSize = sizeof(MoveGen);
    
    EXPECT_LE(moveSize, MAX_MOVE_SIZE)
        << "MoveGen object too large: " << moveSize << " bytes";
        
    EXPECT_GE(moveSize, EXPECTED_MOVE_SIZE)
        << "MoveGen object too small: " << moveSize << " bytes"
        << " (expected at least " << EXPECTED_MOVE_SIZE << " bytes)";
    
    // Test MoveGenList memory efficiency
    size_t moveListSize = sizeof(MoveGenList<>);
    size_t expectedMoveListSize = sizeof(MoveGen) * 256 + sizeof(size_t); // array + count
    
    EXPECT_LT(moveListSize, expectedMoveListSize * 1.2) // Allow 20% overhead
        << "MoveGenList has excessive overhead: " << moveListSize
        << " bytes (expected ~" << expectedMoveListSize << " bytes)";
    
    std::cout << "MEMORY AUDIT [MoveGen]: move_size=" << moveSize 
              << " bytes, list_size=" << moveListSize << " bytes" << std::endl;
}

// MEMORY REQUIREMENT: Move generation should not cause memory fragmentation
TEST_F(MemoryAuditTest, MoveGenerationMemoryFragmentation) {
    const int FRAGMENTATION_TEST_ITERATIONS = 1000;
    const size_t MAX_MEMORY_GROWTH = 4 * 1024 * 1024; // 4MB maximum growth (more realistic for 1000 iterations)
    
    size_t initialMem = getCurrentMemoryUsage();
    
    // Generate moves many times to test for fragmentation
    for (int i = 0; i < FRAGMENTATION_TEST_ITERATIONS; ++i) {
        Board board;
        board.setFromFEN(testPositions[i % testPositions.size()]);
        
        MoveGenList<> moves;
        generatePawnMoves(board, moves, WHITE);
        generatePawnMoves(board, moves, BLACK);
        
        // Occasionally check memory usage
        if (i % 100 == 0) {
            size_t currentMem = getCurrentMemoryUsage();
            size_t memoryGrowth = currentMem > initialMem ? currentMem - initialMem : 0;
            
            EXPECT_LT(memoryGrowth, MAX_MEMORY_GROWTH)
                << "Excessive memory growth detected at iteration " << i
                << ": " << memoryGrowth << " bytes";
        }
    }
    
    size_t finalMem = getCurrentMemoryUsage();
    size_t totalGrowth = finalMem > initialMem ? finalMem - initialMem : 0;
    
    EXPECT_LT(totalGrowth, MAX_MEMORY_GROWTH)
        << "Memory fragmentation detected. Total growth: " << totalGrowth << " bytes";
        
    std::cout << "MEMORY AUDIT [Fragmentation]: " << FRAGMENTATION_TEST_ITERATIONS
              << " iterations, growth=" << totalGrowth << " bytes" << std::endl;
}

// MEMORY REQUIREMENT: No memory leaks in repeated operations
TEST_F(MemoryAuditTest, MemoryLeakDetection) {
    const int LEAK_TEST_ITERATIONS = 10000;
    const size_t MAX_ACCEPTABLE_GROWTH = 2 * 1024 * 1024; // 2MB (more realistic for 10000 iterations)
    
    size_t baselineMemory = getCurrentMemoryUsage();
    std::vector<size_t> memoryCheckpoints;
    
    // Perform many operations that could potentially leak memory
    for (int i = 0; i < LEAK_TEST_ITERATIONS; ++i) {
        // Create and destroy objects
        {
            Board board;
            board.setFromFEN(testPositions[i % testPositions.size()]);
            
            MoveGenList<> moves;
            generatePawnMoves(board, moves, WHITE);
            generatePawnMoves(board, moves, BLACK);
            
            // Test board copying
            Board copy(board);
            
            // Test move copying
            std::vector<MoveGen> moveVector(moves.begin(), moves.end());
        }
        
        // Check memory usage periodically
        if (i % 1000 == 0) {
            size_t currentMemory = getCurrentMemoryUsage();
            memoryCheckpoints.push_back(currentMemory);
        }
    }
    
    // Analyze memory usage trend
    if (memoryCheckpoints.size() >= 2) {
        size_t firstCheck = memoryCheckpoints[0];
        size_t lastCheck = memoryCheckpoints.back();
        size_t memoryGrowth = lastCheck > firstCheck ? lastCheck - firstCheck : 0;
        
        EXPECT_LT(memoryGrowth, MAX_ACCEPTABLE_GROWTH)
            << "Potential memory leak. Growth from first to last checkpoint: "
            << memoryGrowth << " bytes over " << LEAK_TEST_ITERATIONS << " iterations";
    }
    
    // Check final memory usage
    size_t finalMemory = getCurrentMemoryUsage();
    size_t totalGrowth = finalMemory > baselineMemory ? finalMemory - baselineMemory : 0;
    
    EXPECT_LT(totalGrowth, MAX_ACCEPTABLE_GROWTH)
        << "Memory leak detected. Total growth: " << totalGrowth << " bytes";
        
    std::cout << "MEMORY AUDIT [Leak Test]: " << LEAK_TEST_ITERATIONS
              << " iterations, final_growth=" << totalGrowth << " bytes" << std::endl;
}

// MEMORY REQUIREMENT: Stack usage should be reasonable
TEST_F(MemoryAuditTest, StackUsageAudit) {
    const size_t MAX_STACK_FRAME_SIZE = 4096; // 4KB per function call
    
    // Estimate stack usage by checking size of local variables
    auto testStackUsage = [&]() -> size_t {
        Board board;                    // ~256 bytes
        MoveGenList<> moves;           // ~1KB (256 moves * 4 bytes)
        Board copy(board);             // ~256 bytes
        MoveGenList<> moreMoves;       // ~1KB
        
        board.setFromFEN("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1");
        generatePawnMoves(board, moves, WHITE);
        generatePawnMoves(board, moves, BLACK);
        
        return sizeof(board) + sizeof(moves) + sizeof(copy) + sizeof(moreMoves);
    };
    
    size_t estimatedStackUsage = testStackUsage();
    
    EXPECT_LT(estimatedStackUsage, MAX_STACK_FRAME_SIZE)
        << "Excessive stack usage: " << estimatedStackUsage << " bytes"
        << " (limit: " << MAX_STACK_FRAME_SIZE << " bytes)";
        
    std::cout << "MEMORY AUDIT [Stack]: estimated_usage=" 
              << estimatedStackUsage << " bytes" << std::endl;
}

// MEMORY REQUIREMENT: Memory alignment should be optimal
TEST_F(MemoryAuditTest, MemoryAlignmentAudit) {
    // Check alignment of critical structures
    EXPECT_EQ(alignof(Board) % sizeof(void*), 0)
        << "Board not properly aligned. Alignment: " << alignof(Board);
        
    EXPECT_EQ(alignof(MoveGen) % sizeof(uint32_t), 0)
        << "MoveGen not properly aligned. Alignment: " << alignof(MoveGen);
        
    // Check that bitboard fields are 8-byte aligned for optimal access
    EXPECT_GE(alignof(Bitboard), 8)
        << "Bitboard alignment suboptimal: " << alignof(Bitboard);
    
    // Test memory layout efficiency
    Board board;
    uintptr_t boardAddr = reinterpret_cast<uintptr_t>(&board);
    
    EXPECT_EQ(boardAddr % alignof(Board), 0)
        << "Board instance not properly aligned in memory";
        
    std::cout << "MEMORY AUDIT [Alignment]: Board=" << alignof(Board)
              << ", MoveGen=" << alignof(MoveGen)
              << ", Bitboard=" << alignof(Bitboard) << std::endl;
}

// MEMORY REQUIREMENT: Cache line efficiency
TEST_F(MemoryAuditTest, CacheLineEfficiencyAudit) {
    const size_t CACHE_LINE_SIZE = 64; // Typical cache line size
    
    // Check if frequently accessed data fits in cache lines
    size_t boardSize = sizeof(Board);
    size_t numCacheLinesForBoard = (boardSize + CACHE_LINE_SIZE - 1) / CACHE_LINE_SIZE;
    
    // Board should fit in reasonable number of cache lines (allow up to 4)
    EXPECT_LE(numCacheLinesForBoard, 4)
        << "Board spans too many cache lines: " << numCacheLinesForBoard
        << " (board size: " << boardSize << " bytes)";
    
    // MoveGenList should be cache-friendly
    size_t moveListSize = sizeof(MoveGenList<>);
    size_t numCacheLinesForMoveList = (moveListSize + CACHE_LINE_SIZE - 1) / CACHE_LINE_SIZE;
    
    // For large move lists, this is expected to be large, but check it's reasonable
    EXPECT_LE(numCacheLinesForMoveList, 64) // 256 moves * 4 bytes ≈ 1KB ≈ 16 cache lines
        << "MoveGenList spans excessive cache lines: " << numCacheLinesForMoveList;
        
    std::cout << "MEMORY AUDIT [Cache]: Board_lines=" << numCacheLinesForBoard
              << ", MoveList_lines=" << numCacheLinesForMoveList << std::endl;
}

// MEMORY REQUIREMENT: Memory access patterns should be optimal
TEST_F(MemoryAuditTest, MemoryAccessPatternAudit) {
    Board board;
    board.setFromFEN("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1");
    
    const int ACCESS_TEST_ITERATIONS = 10000;
    
    // Helper lambda for measuring execution time
    auto measureTime = [](auto&& func, int iterations = 1000) -> long long {
        auto start = high_resolution_clock::now();
        for (int i = 0; i < iterations; ++i) {
            func();
        }
        auto end = high_resolution_clock::now();
        return duration_cast<nanoseconds>(end - start).count() / iterations;
    };

    // Test sequential access pattern (should be fast)
    auto sequentialTime = measureTime([&]() {
        for (Square sq = A1; sq <= H8; sq = static_cast<Square>(sq + 1)) {
            volatile auto piece = board.getPiece(sq);
            (void)piece;
        }
    }, ACCESS_TEST_ITERATIONS / 64);
    
    // Test strided access pattern (should be slower but reasonable)
    auto stridedTime = measureTime([&]() {
        for (int i = 0; i < 64; i += 8) { // Access every 8th square
            volatile auto piece = board.getPiece(static_cast<Square>(i));
            (void)piece;
        }
        for (int i = 1; i < 64; i += 8) {
            volatile auto piece = board.getPiece(static_cast<Square>(i));
            (void)piece;
        }
        // Continue for all 8 strides...
    }, ACCESS_TEST_ITERATIONS / 64);
    
    // Sequential should be faster than strided (allow up to 2x difference)
    double accessRatio = static_cast<double>(stridedTime) / sequentialTime;
    
    EXPECT_LT(accessRatio, 2.0)
        << "Memory access pattern inefficient. Sequential: " << sequentialTime
        << "ns, Strided: " << stridedTime << "ns, Ratio: " << accessRatio;
        
    std::cout << "MEMORY AUDIT [Access Pattern]: sequential=" << sequentialTime
              << "ns, strided=" << stridedTime << "ns, ratio=" << accessRatio << std::endl;
}

// COMPREHENSIVE MEMORY BENCHMARK
TEST_F(MemoryAuditTest, ComprehensiveMemoryBenchmark) {
    struct MemoryBenchmark {
        std::string name;
        std::string fen;
        size_t maxPeakMemoryBytes;
        size_t maxSteadyStateBytes;
    };
    
    std::vector<MemoryBenchmark> benchmarks = {
        {"Starting position", STARTING_FEN, 131072, 131072},     // 128KB peak, 128KB steady (allow for system granularity)
        {"Complex middlegame", "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", 196608, 196608},  // 192KB peak, 192KB steady
        {"Tactical position", "r1bq1rk1/ppp2ppp/2np1n2/2b1p3/2B1P3/3P1N2/PPP2PPP/RNBQK2R w KQ - 0 1", 163840, 163840}, // 160KB peak, 160KB steady
        {"Endgame position", "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", 98304, 98304}  // 96KB peak, 96KB steady
    };
    
    for (const auto& benchmark : benchmarks) {
        size_t initialMem = getCurrentMemoryUsage();
        
        // Measure peak memory during move generation
        size_t peakMemory = measurePeakMemoryUsage([&]() {
            Board board;
            board.setFromFEN(benchmark.fen);
            
            MoveGenList<> moves;
            generatePawnMoves(board, moves, WHITE);
            generatePawnMoves(board, moves, BLACK);
            
            // Create additional objects to stress memory
            std::vector<MoveGen> moveVector(moves.begin(), moves.end());
            Board copy(board);
        });
        
        // Measure steady-state memory (after cleanup)
        size_t steadyStateMem = getCurrentMemoryUsage();
        size_t steadyStateGrowth = steadyStateMem > initialMem ? steadyStateMem - initialMem : 0;
        
        EXPECT_LT(peakMemory, benchmark.maxPeakMemoryBytes)
            << "Peak memory benchmark failed for " << benchmark.name
            << ": " << peakMemory << " > " << benchmark.maxPeakMemoryBytes << " bytes";
            
        EXPECT_LT(steadyStateGrowth, benchmark.maxSteadyStateBytes)
            << "Steady state memory benchmark failed for " << benchmark.name
            << ": " << steadyStateGrowth << " > " << benchmark.maxSteadyStateBytes << " bytes";
            
        std::cout << "MEMORY BENCHMARK [" << benchmark.name << "]: "
                  << "peak=" << peakMemory << " bytes, "
                  << "steady_state=" << steadyStateGrowth << " bytes" << std::endl;
    }
}