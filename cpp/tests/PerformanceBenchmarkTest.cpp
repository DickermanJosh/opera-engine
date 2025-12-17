/**
 * @file PerformanceBenchmarkTest.cpp
 * @brief Production-grade performance benchmarking suite
 *
 * Validates:
 * - Search performance (nodes/second) across various positions
 * - Move generation speed
 * - Evaluation speed
 * - Memory usage and allocation patterns
 * - Performance regression detection
 */

#include <gtest/gtest.h>
#include <memory>
#include <chrono>
#include <vector>
#include <iostream>
#include <iomanip>
#include "Board.h"
#include "MoveGen.h"
#include "search/search_engine.h"
#include "eval/handcrafted_eval.h"
#include "eval/morphy_eval.h"

using namespace opera;
using namespace std::chrono;

// Performance targets
constexpr uint64_t MIN_NPS = 100000;           // Minimum 100K nodes/second
constexpr uint64_t TARGET_NPS = 500000;        // Target 500K nodes/second
constexpr uint64_t MIN_MOVEGEN_PER_SEC = 1000000;  // 1M move generations/second
constexpr double MAX_EVAL_TIME_US = 1.0;       // Max 1μs per evaluation

/**
 * Benchmark test fixture with timing utilities
 */
class PerformanceBenchmarkTest : public ::testing::Test {
protected:
    void SetUp() override {
        board = std::make_unique<Board>();
        stop_flag.store(false);
    }

    struct BenchmarkResult {
        std::string name;
        uint64_t nodes;
        uint64_t time_ms;
        uint64_t nps;
        bool meets_target;
    };

    void print_result(const BenchmarkResult& result) {
        std::cout << std::fixed << std::setprecision(0);
        std::cout << "\n  " << std::left << std::setw(40) << result.name;
        std::cout << std::right << std::setw(12) << result.nodes << " nodes  ";
        std::cout << std::setw(8) << result.time_ms << " ms  ";
        std::cout << std::setw(12) << result.nps << " nps  ";
        std::cout << (result.meets_target ? "✓" : "✗") << "\n";
    }

    std::unique_ptr<Board> board;
    std::atomic<bool> stop_flag;
};

// ============================================================================
// Search Performance Benchmarks
// ============================================================================

/**
 * Benchmark 1: Starting position search performance
 */
TEST_F(PerformanceBenchmarkTest, StartingPositionSearch) {
    board->setFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    SearchEngine engine(*board, stop_flag);

    SearchLimits limits;
    limits.max_depth = 5;

    auto start = high_resolution_clock::now();
    SearchResult result = engine.search(limits);
    auto end = high_resolution_clock::now();

    auto duration = duration_cast<milliseconds>(end - start);
    uint64_t nps = duration.count() > 0 ? (result.nodes * 1000) / duration.count() : 0;

    BenchmarkResult bench{
        "Starting Position (depth 5)",
        result.nodes,
        static_cast<uint64_t>(duration.count()),
        nps,
        nps >= MIN_NPS
    };
    print_result(bench);

    EXPECT_GT(nps, MIN_NPS);
    EXPECT_GT(result.nodes, 1000);  // Should search at least 1K nodes
}

/**
 * Benchmark 2: Complex middlegame position
 */
TEST_F(PerformanceBenchmarkTest, ComplexMiddlegameSearch) {
    board->setFromFEN("r1bq1rk1/ppp2ppp/2np1n2/2b1p3/2B1P3/2NP1N2/PPP2PPP/R1BQK2R w KQ - 0 8");
    SearchEngine engine(*board, stop_flag);

    SearchLimits limits;
    limits.max_depth = 5;

    auto start = high_resolution_clock::now();
    SearchResult result = engine.search(limits);
    auto end = high_resolution_clock::now();

    auto duration = duration_cast<milliseconds>(end - start);
    uint64_t nps = duration.count() > 0 ? (result.nodes * 1000) / duration.count() : 0;

    BenchmarkResult bench{
        "Complex Middlegame (depth 5)",
        result.nodes,
        static_cast<uint64_t>(duration.count()),
        nps,
        nps >= MIN_NPS
    };
    print_result(bench);

    EXPECT_GT(nps, MIN_NPS);
}

/**
 * Benchmark 3: Tactical position with forcing moves
 */
TEST_F(PerformanceBenchmarkTest, TacticalPositionSearch) {
    // Position with tactical opportunities
    board->setFromFEN("r1bqk2r/ppp2ppp/2n5/2bppn2/2B1P3/2NP1N2/PPP2PPP/R1BQK2R w KQkq - 0 8");
    SearchEngine engine(*board, stop_flag);

    SearchLimits limits;
    limits.max_depth = 5;

    auto start = high_resolution_clock::now();
    SearchResult result = engine.search(limits);
    auto end = high_resolution_clock::now();

    auto duration = duration_cast<milliseconds>(end - start);
    uint64_t nps = duration.count() > 0 ? (result.nodes * 1000) / duration.count() : 0;

    BenchmarkResult bench{
        "Tactical Position (depth 5)",
        result.nodes,
        static_cast<uint64_t>(duration.count()),
        nps,
        nps >= MIN_NPS
    };
    print_result(bench);

    EXPECT_GT(nps, MIN_NPS);
}

/**
 * Benchmark 4: Endgame position
 */
TEST_F(PerformanceBenchmarkTest, EndgameSearch) {
    board->setFromFEN("8/5k2/3p4/1p1Pp2p/pP2Pp1P/P4P1K/8/8 b - - 0 1");
    SearchEngine engine(*board, stop_flag);

    SearchLimits limits;
    limits.max_depth = 6;  // Endgames can search deeper

    auto start = high_resolution_clock::now();
    SearchResult result = engine.search(limits);
    auto end = high_resolution_clock::now();

    auto duration = duration_cast<milliseconds>(end - start);
    uint64_t nps = duration.count() > 0 ? (result.nodes * 1000) / duration.count() : 0;

    BenchmarkResult bench{
        "Endgame Position (depth 6)",
        result.nodes,
        static_cast<uint64_t>(duration.count()),
        nps,
        nps >= MIN_NPS
    };
    print_result(bench);

    EXPECT_GT(nps, MIN_NPS);
}

// ============================================================================
// Move Generation Performance Benchmarks
// ============================================================================

/**
 * Benchmark 5: Move generation speed
 */
TEST_F(PerformanceBenchmarkTest, MoveGenerationSpeed) {
    board->setFromFEN("r1bqkb1r/pppp1ppp/2n2n2/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 0 1");

    constexpr int iterations = 100000;
    auto start = high_resolution_clock::now();

    for (int i = 0; i < iterations; ++i) {
        MoveGenList<> moves;
        generateAllLegalMoves(*board, moves, board->getSideToMove());
    }

    auto end = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(end - start);

    uint64_t movegen_per_sec = duration.count() > 0 ?
        (static_cast<uint64_t>(iterations) * 1000000) / duration.count() : 0;

    double avg_time_us = static_cast<double>(duration.count()) / iterations;

    std::cout << "\n  Move Generation Speed:\n";
    std::cout << "    Iterations: " << iterations << "\n";
    std::cout << "    Total time: " << duration.count() << " μs\n";
    std::cout << "    Average: " << std::fixed << std::setprecision(2) << avg_time_us << " μs/gen\n";
    std::cout << "    Rate: " << movegen_per_sec << " generations/sec\n";

    EXPECT_GT(movegen_per_sec, MIN_MOVEGEN_PER_SEC);
    EXPECT_LT(avg_time_us, 1.0);  // Should be < 1μs per generation
}

/**
 * Benchmark 6: Move generation in complex position
 */
TEST_F(PerformanceBenchmarkTest, ComplexPositionMoveGen) {
    // Position with many pieces and move possibilities
    board->setFromFEN("r1bq1rk1/ppp2ppp/2np1n2/2b1p3/2B1P3/2NP1N2/PPP2PPP/R1BQK2R w KQ - 0 8");

    constexpr int iterations = 50000;
    auto start = high_resolution_clock::now();

    for (int i = 0; i < iterations; ++i) {
        MoveGenList<> moves;
        generateAllLegalMoves(*board, moves, board->getSideToMove());
    }

    auto end = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(end - start);

    double avg_time_us = static_cast<double>(duration.count()) / iterations;

    std::cout << "\n  Complex Position Move Generation:\n";
    std::cout << "    Average: " << std::fixed << std::setprecision(2) << avg_time_us << " μs/gen\n";

    EXPECT_LT(avg_time_us, 2.0);  // Should be < 2μs even in complex positions
}

// ============================================================================
// Evaluation Performance Benchmarks
// ============================================================================

/**
 * Benchmark 7: HandcraftedEvaluator speed
 */
TEST_F(PerformanceBenchmarkTest, HandcraftedEvaluatorSpeed) {
    board->setFromFEN("r1bqkb1r/pppp1ppp/2n2n2/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 0 1");
    eval::HandcraftedEvaluator evaluator;

    constexpr int iterations = 100000;
    auto start = high_resolution_clock::now();

    int total_score = 0;
    for (int i = 0; i < iterations; ++i) {
        total_score += evaluator.evaluate(*board, WHITE);
    }

    auto end = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(end - start);

    double avg_time_us = static_cast<double>(duration.count()) / iterations;

    std::cout << "\n  HandcraftedEvaluator Speed:\n";
    std::cout << "    Iterations: " << iterations << "\n";
    std::cout << "    Average: " << std::fixed << std::setprecision(3) << avg_time_us << " μs/eval\n";
    std::cout << "    Dummy sum: " << total_score << " (prevent optimization)\n";

    EXPECT_LT(avg_time_us, MAX_EVAL_TIME_US);
}

/**
 * Benchmark 8: MorphyEvaluator speed
 */
TEST_F(PerformanceBenchmarkTest, MorphyEvaluatorSpeed) {
    board->setFromFEN("r1bqkb1r/pppp1ppp/2n2n2/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 0 1");
    eval::MorphyEvaluator evaluator(1.2);

    constexpr int iterations = 100000;
    auto start = high_resolution_clock::now();

    int total_score = 0;
    for (int i = 0; i < iterations; ++i) {
        total_score += evaluator.evaluate(*board, WHITE);
    }

    auto end = high_resolution_clock::now();
    auto duration = duration_cast<microseconds>(end - start);

    double avg_time_us = static_cast<double>(duration.count()) / iterations;

    std::cout << "\n  MorphyEvaluator Speed:\n";
    std::cout << "    Iterations: " << iterations << "\n";
    std::cout << "    Average: " << std::fixed << std::setprecision(3) << avg_time_us << " μs/eval\n";
    std::cout << "    Dummy sum: " << total_score << " (prevent optimization)\n";

    EXPECT_LT(avg_time_us, MAX_EVAL_TIME_US * 1.5);  // Allow slightly slower for Morphy
}

// ============================================================================
// Memory and Scaling Benchmarks
// ============================================================================

/**
 * Benchmark 9: Search node scaling
 */
TEST_F(PerformanceBenchmarkTest, SearchNodeScaling) {
    board->setFromFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
    SearchEngine engine(*board, stop_flag);

    std::cout << "\n  Search Node Scaling:\n";
    std::cout << "  Depth | Nodes      | Time(ms) | NPS        | Target Met\n";
    std::cout << "  ------|------------|----------|------------|------------\n";

    for (int depth = 3; depth <= 5; ++depth) {
        stop_flag.store(false);
        SearchLimits limits;
        limits.max_depth = depth;

        auto start = high_resolution_clock::now();
        SearchResult result = engine.search(limits);
        auto end = high_resolution_clock::now();

        auto duration = duration_cast<milliseconds>(end - start);
        uint64_t nps = duration.count() > 0 ? (result.nodes * 1000) / duration.count() : 0;

        std::cout << "  " << std::setw(5) << depth << " | ";
        std::cout << std::setw(10) << result.nodes << " | ";
        std::cout << std::setw(8) << duration.count() << " | ";
        std::cout << std::setw(10) << nps << " | ";
        std::cout << (nps >= MIN_NPS ? "✓" : "✗") << "\n";

        EXPECT_GT(nps, MIN_NPS);
        engine.reset_statistics();
    }
}

/**
 * Benchmark 10: Consistent performance across positions
 */
TEST_F(PerformanceBenchmarkTest, ConsistentPerformance) {
    std::vector<std::string> test_positions = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r1bqkb1r/pppp1ppp/2n2n2/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 0 1",
        "r1bq1rk1/ppp2ppp/2np1n2/2b1p3/2B1P3/2NP1N2/PPP2PPP/R1BQK2R w KQ - 0 8",
        "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
    };

    std::cout << "\n  Performance Consistency Test:\n";

    uint64_t total_nps = 0;
    int position_count = 0;

    for (const auto& fen : test_positions) {
        board->setFromFEN(fen);
        stop_flag.store(false);
        SearchEngine engine(*board, stop_flag);

        SearchLimits limits;
        limits.max_depth = 4;

        auto start = high_resolution_clock::now();
        SearchResult result = engine.search(limits);
        auto end = high_resolution_clock::now();

        auto duration = duration_cast<milliseconds>(end - start);
        uint64_t nps = duration.count() > 0 ? (result.nodes * 1000) / duration.count() : 0;

        total_nps += nps;
        position_count++;

        EXPECT_GT(nps, MIN_NPS);
    }

    uint64_t avg_nps = total_nps / position_count;
    std::cout << "    Average NPS across " << position_count << " positions: " << avg_nps << "\n";
    std::cout << "    Minimum target: " << MIN_NPS << " nps\n";

    EXPECT_GT(avg_nps, MIN_NPS);
}
