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
BUILD_FIRST=false
SHOW_HELP=false
DEBUG_MODE=false

# Parse command line arguments
for arg in "$@"; do
    case $arg in
        --test)
            RUN_TESTS=true
            shift
            ;;
        --performance)
            RUN_PERFORMANCE_TESTS=true
            shift
            ;;
        --build)
            BUILD_FIRST=true
            shift
            ;;
        --debug)
            DEBUG_MODE=true
            BUILD_TYPE="Debug"
            shift
            ;;
        --help|-h)
            SHOW_HELP=true
            shift
            ;;
        *)
            echo -e "${RED}Unknown option: $arg${NC}"
            SHOW_HELP=true
            ;;
    esac
done

# Show help
if [ "$SHOW_HELP" = true ]; then
    echo -e "${BLUE}Opera Chess Engine Launch Script${NC}"
    echo ""
    echo "Usage: ./launch.sh [options]"
    echo ""
    echo "Options:"
    echo "  --test          Run test suite instead of normal application"
    echo "  --performance   Run comprehensive performance test suite"
    echo "  --build         Force rebuild before running"
    echo "  --debug         Build and run in debug mode"
    echo "  --help, -h      Show this help message"
    echo ""
    echo "Examples:"
    echo "  ./launch.sh                    # Run normal application"
    echo "  ./launch.sh --test             # Run test suite"
    echo "  ./launch.sh --performance      # Run performance tests"
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
if [ "$BUILD_FIRST" = true ] || [ ! -f "opera-engine" ] || ([ "$RUN_TESTS" = true ] && [ ! -f "tests/opera_tests" ]) || ([ "$RUN_PERFORMANCE_TESTS" = true ] && [ ! -f "tests/opera_tests" ]); then
    echo -e "${YELLOW}Building Opera Engine ($BUILD_TYPE mode)...${NC}"
    
    # Configure with CMake
    if [ "$RUN_TESTS" = true ] || [ "$RUN_PERFORMANCE_TESTS" = true ]; then
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
if [ "$RUN_PERFORMANCE_TESTS" = true ]; then
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
elif [ "$RUN_TESTS" = true ]; then
    echo -e "${BLUE}🧪 Running Test Suite...${NC}"
    echo "========================"
    
    if [ -f "tests/opera_tests" ]; then
        # Run tests directly for better output control
        ./tests/opera_tests
        TEST_RESULT=$?
        
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