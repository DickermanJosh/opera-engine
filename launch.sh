#!/bin/bash

# Opera Engine Launch Script
# Usage: ./launch.sh [--test] [--build] [--debug] [--help]

set -e  # Exit on any error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
BLUE='\033[0;34m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Default values
BUILD_TYPE="Release"
RUN_TESTS=false
RUN_PERFORMANCE_TESTS=false
RUN_PERFT=false
RUN_SEARCH_EVAL=false
RUN_SEARCH_BENCH=false
PERFT_FEN=""
PERFT_DEPTH=0
BUILD_FIRST=false
SHOW_HELP=false
DEBUG_MODE=false
CI_MODE=false

# Parse command line arguments
i=1
while [ $i -le $# ]; do
    arg="${!i}"
    case $arg in
        --test)
            RUN_TESTS=true
            ;;
        --ci)
            RUN_TESTS=true
            CI_MODE=true
            ;;
        --performance)
            RUN_PERFORMANCE_TESTS=true
            ;;
        --search-eval)
            RUN_SEARCH_EVAL=true
            ;;
        --search-bench)
            RUN_SEARCH_BENCH=true
            ;;
        --perft)
            RUN_PERFT=true
            # Check if next argument is a FEN string (quoted)
            next_i=$((i + 1))
            if [ $next_i -le $# ]; then
                next_arg="${!next_i}"
                # If next arg doesn't start with --, treat as FEN
                if [[ ! "$next_arg" =~ ^-- ]]; then
                    PERFT_FEN="$next_arg"
                    i=$((i + 1))
                    # Check for depth argument
                    depth_i=$((i + 1))
                    if [ $depth_i -le $# ]; then
                        depth_arg="${!depth_i}"
                        if [[ "$depth_arg" =~ ^[0-9]+$ ]]; then
                            PERFT_DEPTH="$depth_arg"
                            i=$((i + 1))
                        fi
                    fi
                fi
            fi
            ;;
        --build)
            BUILD_FIRST=true
            ;;
        --debug)
            DEBUG_MODE=true
            BUILD_TYPE="Debug"
            ;;
        --help|-h)
            SHOW_HELP=true
            ;;
        *)
            # Don't show error for FEN strings or depths that were already parsed
            if [ "$RUN_PERFT" = false ]; then
                echo -e "${RED}Unknown option: $arg${NC}"
                SHOW_HELP=true
            fi
            ;;
    esac
    i=$((i + 1))
done

# Show help
if [ "$SHOW_HELP" = true ]; then
    echo -e "${BLUE}Opera Chess Engine Launch Script${NC}"
    echo ""
    echo "Usage: ./launch.sh [options]"
    echo ""
    echo "Options:"
    echo "  --test          Run test suite instead of normal application"
    echo "  --ci            Run core tests only (excludes performance tests, ideal for CI)"
    echo "  --performance   Run comprehensive performance test suite"
    echo "  --search-eval   Run search and evaluation test suite"
    echo "  --search-bench  Run search and evaluation performance benchmarks"
    echo "  --perft         Run Perft validation suite"
    echo "  --perft \"FEN\" D   Run Perft on specific position to depth D"
    echo "  --build         Force rebuild before running"
    echo "  --debug         Build and run in debug mode"
    echo "  --help, -h      Show this help message"
    echo ""
    echo "Examples:"
    echo "  ./launch.sh                    # Run normal application"
    echo "  ./launch.sh --test             # Run all tests"
    echo "  ./launch.sh --ci               # Run core tests only (for CI/CD)"
    echo "  ./launch.sh --performance      # Run performance tests"
    echo "  ./launch.sh --search-eval      # Run search/eval tests"
    echo "  ./launch.sh --search-bench     # Run search/eval benchmarks"
    echo "  ./launch.sh --perft            # Run Perft validation suite"
    echo "  ./launch.sh --perft \"FEN\" 5     # Test specific position to depth 5"
    echo "  ./launch.sh --build --test     # Rebuild and run tests"
    echo "  ./launch.sh --debug            # Run in debug mode"
    exit 0
fi

echo -e "${BLUE}🎼 Opera Chess Engine Launcher${NC}"
echo "================================="

# Change to cpp directory
cd cpp

# Check if build directory exists
if [ ! -d "build" ]; then
    echo -e "${YELLOW}Creating build directory...${NC}"
    mkdir -p build
    BUILD_FIRST=true
fi

cd build

# Build if requested or if executables don't exist
if [ "$BUILD_FIRST" = true ] || [ ! -f "opera-engine" ] || ([ "$RUN_TESTS" = true ] && [ ! -f "tests/opera_tests" ]) || ([ "$RUN_PERFORMANCE_TESTS" = true ] && [ ! -f "tests/opera_tests" ]) || ([ "$RUN_SEARCH_EVAL" = true ] && [ ! -f "tests/opera_tests" ]) || ([ "$RUN_SEARCH_BENCH" = true ] && [ ! -f "tests/opera_tests" ]) || ([ "$RUN_PERFT" = true ] && [ ! -f "perft-runner" ]); then
    echo -e "${YELLOW}Building Opera Engine ($BUILD_TYPE mode)...${NC}"

    # Configure with CMake
    if [ "$RUN_TESTS" = true ] || [ "$RUN_PERFORMANCE_TESTS" = true ] || [ "$RUN_SEARCH_EVAL" = true ] || [ "$RUN_SEARCH_BENCH" = true ]; then
        cmake .. -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DBUILD_TESTS=ON
    else
        cmake .. -DCMAKE_BUILD_TYPE=$BUILD_TYPE -DBUILD_TESTS=OFF
    fi
    
    # Build
    make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
    
    if [ $? -eq 0 ]; then
        echo -e "${GREEN}✅ Build successful!${NC}"
    else
        echo -e "${RED}❌ Build failed!${NC}"
        exit 1
    fi
    echo ""
fi

# Run based on mode
if [ "$RUN_PERFT" = true ]; then
    echo -e "${BLUE}🎯 Running Perft Validation...${NC}"
    echo "=============================="
    
    if [ -f "perft-runner" ]; then
        if [ -n "$PERFT_FEN" ] && [ "$PERFT_DEPTH" -gt 0 ]; then
            echo -e "${YELLOW}Testing custom position to depth $PERFT_DEPTH...${NC}"
            ./perft-runner "$PERFT_FEN" "$PERFT_DEPTH"
        else
            echo -e "${YELLOW}Running full Perft test suite...${NC}"
            ./perft-runner
        fi
        PERFT_RESULT=$?
        
        echo ""
        if [ $PERFT_RESULT -eq 0 ]; then
            echo -e "${GREEN}🎉 Perft validation completed successfully!${NC}"
        else
            echo -e "${RED}💥 Perft validation failed. Exit code: $PERFT_RESULT${NC}"
            exit $PERFT_RESULT
        fi
    else
        echo -e "${RED}❌ Perft runner executable not found! Try running with --build flag.${NC}"
        exit 1
    fi
elif [ "$RUN_PERFORMANCE_TESTS" = true ]; then
    echo -e "${BLUE}🚀 Running Performance Test Suite...${NC}"
    echo "=================================="
    
    if [ -f "tests/opera_tests" ]; then
        # Check if performance test script exists
        if [ -f "../cpp/run_performance_tests.sh" ]; then
            echo -e "${YELLOW}Using dedicated performance test runner...${NC}"
            cd ../cpp
            chmod +x run_performance_tests.sh
            ./run_performance_tests.sh
            PERF_RESULT=$?
            cd build
        else
            echo -e "${YELLOW}Running performance tests directly...${NC}"
            # Run performance-specific test filters
            ./tests/opera_tests --gtest_filter="PerformanceTest.*:BoardPerformanceTest.*" --gtest_color=yes
            PERF_RESULT=$?
        fi
        
        echo ""
        if [ $PERF_RESULT -eq 0 ]; then
            echo -e "${GREEN}🎉 All performance tests passed successfully!${NC}"
        else
            echo -e "${RED}💥 Some performance tests failed. Exit code: $PERF_RESULT${NC}"
            exit $PERF_RESULT
        fi
    else
        echo -e "${RED}❌ Test executable not found! Try running with --build flag.${NC}"
        exit 1
    fi
elif [ "$RUN_SEARCH_EVAL" = true ]; then
    echo -e "${BLUE}🔍 Running Search and Evaluation Test Suite...${NC}"
    echo "==============================================="

    if [ -f "tests/opera_tests" ]; then
        echo -e "${YELLOW}Running search and evaluation tests...${NC}"
        # Run all search, evaluation, and integration tests
        ./tests/opera_tests --gtest_filter="SearchEngineTest.*:AlphaBetaTest.*:TranspositionTableTest.*:MoveOrderingTest.*:StaticExchangeTest.*:SearchOptimizationTest.*:UCIOptionsTest.*:SearchControlTest.*:EvaluatorInterfaceTest.*:HandcraftedEvalTest.*:AdvancedEvalTest.*:MorphyEvalTest.*:EvalCachingTest.*:SearchEvalIntegrationTest.*:TacticalEPDTest.*:MorphyStyleValidationTest.*" --gtest_color=yes
        SEARCH_EVAL_RESULT=$?

        echo ""
        if [ $SEARCH_EVAL_RESULT -eq 0 ]; then
            echo -e "${GREEN}🎉 All search and evaluation tests passed successfully!${NC}"
        else
            echo -e "${RED}💥 Some search/eval tests failed. Exit code: $SEARCH_EVAL_RESULT${NC}"
            exit $SEARCH_EVAL_RESULT
        fi
    else
        echo -e "${RED}❌ Test executable not found! Try running with --build flag.${NC}"
        exit 1
    fi
elif [ "$RUN_SEARCH_BENCH" = true ]; then
    echo -e "${BLUE}⚡ Running Search and Evaluation Performance Benchmarks...${NC}"
    echo "=========================================================="

    if [ -f "tests/opera_tests" ]; then
        echo -e "${YELLOW}Running performance benchmarks...${NC}"
        # Run performance benchmark tests
        ./tests/opera_tests --gtest_filter="PerformanceBenchmarkTest.*" --gtest_color=yes
        SEARCH_BENCH_RESULT=$?

        echo ""
        if [ $SEARCH_BENCH_RESULT -eq 0 ]; then
            echo -e "${GREEN}🎉 All benchmarks completed successfully!${NC}"
        else
            echo -e "${RED}💥 Some benchmarks failed. Exit code: $SEARCH_BENCH_RESULT${NC}"
            exit $SEARCH_BENCH_RESULT
        fi
    else
        echo -e "${RED}❌ Test executable not found! Try running with --build flag.${NC}"
        exit 1
    fi
elif [ "$RUN_TESTS" = true ]; then
    if [ "$CI_MODE" = true ]; then
        echo -e "${BLUE}🧪 Running Core Test Suite (CI Mode - No Performance Tests)...${NC}"
        echo "=================================================================="
    else
        echo -e "${BLUE}🧪 Running Test Suite...${NC}"
        echo "========================"
    fi
    
    if [ -f "tests/opera_tests" ]; then
        if [ "$CI_MODE" = true ]; then
            # Run only core tests, exclude performance and memory audit tests
            echo -e "${YELLOW}Excluding performance and memory intensive tests for CI${NC}"
            ./tests/opera_tests --gtest_filter="-PerformanceTest.*:MemoryAuditTest.*:BoardPerformanceTest.*" --gtest_color=yes
            TEST_RESULT=$?
        else
            # Run all tests
            ./tests/opera_tests
            TEST_RESULT=$?
        fi
        
        echo ""
        if [ $TEST_RESULT -eq 0 ]; then
            echo -e "${GREEN}🎉 All tests passed successfully!${NC}"
        else
            echo -e "${RED}💥 Some tests failed. Exit code: $TEST_RESULT${NC}"
            exit $TEST_RESULT
        fi
    else
        echo -e "${RED}❌ Test executable not found! Try running with --build flag.${NC}"
        exit 1
    fi
else
    echo -e "${BLUE}🚀 Launching Opera Engine...${NC}"
    echo "============================="
    
    if [ -f "opera-engine" ]; then
        ./opera-engine
    else
        echo -e "${RED}❌ Opera engine executable not found! Try running with --build flag.${NC}"
        exit 1
    fi
fi

echo ""
echo -e "${GREEN}🎼 Opera Engine session completed.${NC}"