#!/bin/bash
# Production-grade test runner with comprehensive reporting
# Runs all tests, generates coverage, and produces formatted reports

set -e  # Exit on error

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
BUILD_DIR="cpp/build"
TEST_BINARY="$BUILD_DIR/tests/opera_tests"
COVERAGE_DIR="$BUILD_DIR/coverage"
REPORT_DIR="test_reports"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")

# Test categories
declare -A TEST_CATEGORIES=(
    ["core"]="BoardTest|MoveGenTest|PawnMoveTest|KnightMoveTest|SlidingPieceTest|KingMoveTest|ChessRulesTest"
    ["performance"]="PerformanceTest|BoardPerformanceTest|PerformanceBenchmarkTest"
    ["search"]="SearchEngineTest|AlphaBetaTest|SearchOptimizationTest|SearchControlTest|TranspositionTableTest|MoveOrderingTest|StaticExchangeTest"
    ["eval"]="EvaluatorInterfaceTest|HandcraftedEvalTest|MorphyEvalTest|EvalCachingTest|AdvancedEvalTest"
    ["integration"]="SearchEvalIntegrationTest"
    ["tactical"]="TacticalEPDTest"
    ["memory"]="MemoryAuditTest"
)

# Print header
print_header() {
    echo -e "${BLUE}========================================${NC}"
    echo -e "${BLUE}  Opera Engine Test Suite Runner${NC}"
    echo -e "${BLUE}  Timestamp: $(date)${NC}"
    echo -e "${BLUE}========================================${NC}\n"
}

# Print section
print_section() {
    echo -e "\n${YELLOW}>>> $1${NC}\n"
}

# Check if build exists
check_build() {
    if [ ! -f "$TEST_BINARY" ]; then
        echo -e "${RED}Error: Test binary not found at $TEST_BINARY${NC}"
        echo "Please run 'cmake --build cpp/build' first"
        exit 1
    fi
}

# Run specific test category
run_test_category() {
    local category=$1
    local filter=$2

    print_section "Running $category tests"

    if [ -z "$filter" ]; then
        # Run all tests
        $TEST_BINARY --gtest_color=yes 2>&1 | tee "$REPORT_DIR/${category}_${TIMESTAMP}.log"
    else
        # Run filtered tests
        $TEST_BINARY --gtest_filter="*$filter*" --gtest_color=yes 2>&1 | tee "$REPORT_DIR/${category}_${TIMESTAMP}.log"
    fi

    local exit_code=$?
    if [ $exit_code -eq 0 ]; then
        echo -e "${GREEN}✓ $category tests passed${NC}"
    else
        echo -e "${RED}✗ $category tests failed (exit code: $exit_code)${NC}"
    fi

    return $exit_code
}

# Run all tests
run_all_tests() {
    print_section "Running full test suite"

    # Run with timeout
    timeout 600 $TEST_BINARY --gtest_color=yes 2>&1 | tee "$REPORT_DIR/full_suite_${TIMESTAMP}.log"

    local exit_code=$?
    if [ $exit_code -eq 124 ]; then
        echo -e "${YELLOW}⚠ Test suite timed out after 10 minutes${NC}"
    elif [ $exit_code -eq 0 ]; then
        echo -e "${GREEN}✓ All tests passed${NC}"
    else
        echo -e "${RED}✗ Some tests failed (exit code: $exit_code)${NC}"
    fi

    return $exit_code
}

# Generate coverage report
generate_coverage() {
    print_section "Generating code coverage report"

    if ! command -v lcov &> /dev/null; then
        echo -e "${YELLOW}⚠ lcov not installed, skipping coverage generation${NC}"
        echo "  Install with: sudo apt-get install lcov"
        return 0
    fi

    mkdir -p "$COVERAGE_DIR"

    # Capture coverage
    echo "Capturing coverage data..."
    lcov --capture --directory "$BUILD_DIR" --output-file "$COVERAGE_DIR/coverage.info" 2>&1 | grep -v "ignoring data"

    # Filter out system files and test files
    echo "Filtering coverage data..."
    lcov --remove "$COVERAGE_DIR/coverage.info" \
        '/usr/*' \
        '*/tests/*' \
        '*/build/_deps/*' \
        --output-file "$COVERAGE_DIR/coverage_filtered.info" 2>&1 | grep -v "ignoring data"

    # Generate HTML report
    echo "Generating HTML report..."
    genhtml "$COVERAGE_DIR/coverage_filtered.info" \
        --output-directory "$COVERAGE_DIR/html" \
        --title "Opera Engine Coverage Report" \
        --legend \
        --show-details 2>&1 | tail -10

    # Print summary
    echo -e "\n${GREEN}Coverage report generated:${NC}"
    echo "  HTML: file://$PWD/$COVERAGE_DIR/html/index.html"

    # Print coverage summary
    echo -e "\n${BLUE}Coverage Summary:${NC}"
    lcov --summary "$COVERAGE_DIR/coverage_filtered.info" 2>&1 | grep -E "lines|functions"

    # Extract coverage percentage
    local coverage=$(lcov --summary "$COVERAGE_DIR/coverage_filtered.info" 2>&1 | grep "lines" | awk '{print $2}' | tr -d '%')

    if (( $(echo "$coverage >= 95" | bc -l) )); then
        echo -e "${GREEN}✓ Coverage target met: ${coverage}% >= 95%${NC}"
    else
        echo -e "${YELLOW}⚠ Coverage below target: ${coverage}% < 95%${NC}"
    fi
}

# Run performance benchmarks
run_benchmarks() {
    print_section "Running performance benchmarks"

    $TEST_BINARY --gtest_filter="*PerformanceBenchmark*" --gtest_color=yes 2>&1 | tee "$REPORT_DIR/benchmarks_${TIMESTAMP}.log"

    echo -e "\n${GREEN}Benchmark results saved to: $REPORT_DIR/benchmarks_${TIMESTAMP}.log${NC}"
}

# Run tactical tests
run_tactical() {
    print_section "Running tactical puzzle tests"

    $TEST_BINARY --gtest_filter="*TacticalEPD*" --gtest_color=yes 2>&1 | tee "$REPORT_DIR/tactical_${TIMESTAMP}.log"

    # Extract solve rate if available
    local solve_rate=$(grep "Results:" "$REPORT_DIR/tactical_${TIMESTAMP}.log" | tail -1 | grep -oP '\d+\.\d+%' || echo "N/A")

    echo -e "\n${BLUE}Tactical solve rate: ${solve_rate}${NC}"
    echo -e "${GREEN}Tactical results saved to: $REPORT_DIR/tactical_${TIMESTAMP}.log${NC}"
}

# Check for memory leaks
check_memory_leaks() {
    print_section "Checking for memory leaks"

    if ! command -v valgrind &> /dev/null; then
        echo -e "${YELLOW}⚠ valgrind not installed, skipping memory leak detection${NC}"
        echo "  Install with: sudo apt-get install valgrind"
        return 0
    fi

    echo "Running quick memory check on core tests..."
    timeout 300 valgrind --leak-check=summary --error-exitcode=1 \
        $TEST_BINARY --gtest_filter="*Board*:*MoveGen*" 2>&1 | tee "$REPORT_DIR/memory_check_${TIMESTAMP}.log"

    local exit_code=$?
    if [ $exit_code -eq 0 ]; then
        echo -e "${GREEN}✓ No memory leaks detected${NC}"
    elif [ $exit_code -eq 124 ]; then
        echo -e "${YELLOW}⚠ Memory check timed out${NC}"
    else
        echo -e "${RED}✗ Memory leaks detected!${NC}"
        echo "  See: $REPORT_DIR/memory_check_${TIMESTAMP}.log"
    fi
}

# Generate summary report
generate_summary() {
    print_section "Test Suite Summary"

    local summary_file="$REPORT_DIR/summary_${TIMESTAMP}.txt"

    cat > "$summary_file" << EOF
Opera Engine Test Suite Summary
================================
Timestamp: $(date)

Test Results:
EOF

    # Count test results from logs
    for category in "${!TEST_CATEGORIES[@]}"; do
        local log_file="$REPORT_DIR/${category}_${TIMESTAMP}.log"
        if [ -f "$log_file" ]; then
            local passed=$(grep -c "PASSED" "$log_file" || echo "0")
            local failed=$(grep -c "FAILED" "$log_file" || echo "0")
            echo "  $category: $passed passed, $failed failed" >> "$summary_file"
        fi
    done

    echo "" >> "$summary_file"
    echo "Reports generated in: $REPORT_DIR" >> "$summary_file"
    echo "Coverage report: $COVERAGE_DIR/html/index.html" >> "$summary_file"

    cat "$summary_file"
    echo -e "\n${GREEN}Summary saved to: $summary_file${NC}"
}

# Main execution
main() {
    print_header

    # Create report directory
    mkdir -p "$REPORT_DIR"

    # Check build
    check_build

    # Parse command line arguments
    case "${1:-all}" in
        "all")
            run_all_tests
            generate_coverage
            ;;
        "quick")
            run_test_category "core" "${TEST_CATEGORIES[core]}"
            ;;
        "benchmarks")
            run_benchmarks
            ;;
        "tactical")
            run_tactical
            ;;
        "coverage")
            run_all_tests
            generate_coverage
            ;;
        "memory")
            check_memory_leaks
            ;;
        "core"|"performance"|"search"|"eval"|"integration")
            run_test_category "$1" "${TEST_CATEGORIES[$1]}"
            ;;
        "ci")
            # CI mode: run all tests, generate coverage, check memory
            print_section "Running in CI mode"
            run_all_tests
            generate_coverage
            check_memory_leaks
            generate_summary
            ;;
        *)
            echo "Usage: $0 [all|quick|benchmarks|tactical|coverage|memory|core|performance|search|eval|integration|ci]"
            echo ""
            echo "Options:"
            echo "  all          - Run full test suite (default)"
            echo "  quick        - Run only core tests"
            echo "  benchmarks   - Run performance benchmarks"
            echo "  tactical     - Run tactical puzzle tests"
            echo "  coverage     - Run tests and generate coverage"
            echo "  memory       - Run memory leak detection"
            echo "  ci           - Full CI pipeline (all + coverage + memory)"
            echo "  core         - Run core tests only"
            echo "  performance  - Run performance tests only"
            echo "  search       - Run search tests only"
            echo "  eval         - Run evaluation tests only"
            echo "  integration  - Run integration tests only"
            exit 1
            ;;
    esac

    echo -e "\n${BLUE}========================================${NC}"
    echo -e "${BLUE}  Test run complete!${NC}"
    echo -e "${BLUE}========================================${NC}"
}

# Run main
main "$@"
