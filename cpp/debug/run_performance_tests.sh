#!/bin/bash

# Opera Chess Engine Performance Test Runner
# This script runs comprehensive performance tests with detailed reporting

set -e  # Exit on any error

echo "===========================================" 
echo "Opera Chess Engine Performance Test Suite"
echo "==========================================="
echo

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
BUILD_DIR="build"
TEST_EXECUTABLE="opera_tests"
PERFORMANCE_LOG="performance_results.log"
MEMORY_LOG="memory_audit.log"

# Performance thresholds (conservative for high-performance engine)
MAX_PAWN_GENERATION_TIME_US=10     # 10 microseconds
MAX_BOARD_COPY_TIME_US=1           # 1 microsecond  
MAX_FEN_PARSING_TIME_US=5          # 5 microseconds
MAX_MEMORY_LEAK_KB=1024            # 1MB maximum leak
MAX_BOARD_SIZE_BYTES=256           # 256 bytes per board

# Functions
print_header() {
    echo -e "${BLUE}=== $1 ===${NC}"
}

print_success() {
    echo -e "${GREEN}✓ $1${NC}"
}

print_warning() {
    echo -e "${YELLOW}⚠ $1${NC}"
}

print_error() {
    echo -e "${RED}✗ $1${NC}"
}

check_prerequisites() {
    print_header "Checking Prerequisites"
    
    if [ ! -d "$BUILD_DIR" ]; then
        print_error "Build directory not found. Please run cmake first."
        exit 1
    fi
    
    if [ ! -f "$BUILD_DIR/tests/$TEST_EXECUTABLE" ]; then
        print_error "Test executable not found. Please build tests first."
        echo "Looking for: $BUILD_DIR/tests/$TEST_EXECUTABLE"
        exit 1
    fi
    
    print_success "Prerequisites verified"
    echo
}

run_performance_tests() {
    print_header "Running Performance Tests"
    
    cd "$BUILD_DIR/tests"
    
    # Create log files
    echo "Opera Engine Performance Test Results - $(date)" > "$PERFORMANCE_LOG"
    echo "Opera Engine Memory Audit Results - $(date)" > "$MEMORY_LOG"
    
    # Run pawn move generation performance tests
    echo "Running pawn move generation performance tests..."
    if ./"$TEST_EXECUTABLE" --gtest_filter="PerformanceTest.PawnMoveGeneration*" --gtest_output=xml:pawn_perf.xml; then
        print_success "Pawn move generation performance tests passed"
    else
        print_error "Pawn move generation performance tests failed"
        return 1
    fi
    
    # Run board performance tests
    echo "Running board performance tests..."
    if ./"$TEST_EXECUTABLE" --gtest_filter="BoardPerformanceTest.*" --gtest_output=xml:board_perf.xml; then
        print_success "Board performance tests passed"
    else
        print_error "Board performance tests failed"
        return 1
    fi
    
    # Run memory audit tests
    echo "Running memory audit tests..."
    if ./"$TEST_EXECUTABLE" --gtest_filter="MemoryAuditTest.*" --gtest_output=xml:memory_audit.xml; then
        print_success "Memory audit tests passed"
    else
        print_error "Memory audit tests failed"
        return 1
    fi
    
    cd ../..
    echo
}

run_stress_tests() {
    print_header "Running Stress Tests"
    
    cd "$BUILD_DIR/tests"
    
    # Run extended stress tests with higher iteration counts
    echo "Running extended stress tests (this may take a few minutes)..."
    
    # Set environment variables for stress testing
    export OPERA_STRESS_TEST_ITERATIONS=100000
    export OPERA_MEMORY_TEST_ITERATIONS=50000
    
    if ./"$TEST_EXECUTABLE" --gtest_filter="*StressTest*" --gtest_output=xml:stress_test.xml; then
        print_success "Stress tests passed"
    else
        print_warning "Some stress tests failed (may indicate performance regression)"
    fi
    
    cd ../..
    echo
}

check_memory_usage() {
    print_header "Memory Usage Analysis"
    
    cd "$BUILD_DIR/tests"
    
    # Run memory usage analysis with Valgrind if available
    if command -v valgrind &> /dev/null; then
        echo "Running Valgrind memory check..."
        valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all \
                 --track-origins=yes --log-file=valgrind_output.log \
                 ./"$TEST_EXECUTABLE" --gtest_filter="MemoryAuditTest.MemoryLeakDetection" \
                 > /dev/null 2>&1
        
        if grep -q "definitely lost: 0 bytes" valgrind_output.log && \
           grep -q "indirectly lost: 0 bytes" valgrind_output.log; then
            print_success "No memory leaks detected by Valgrind"
        else
            print_warning "Potential memory issues detected by Valgrind"
            echo "Check valgrind_output.log for details"
        fi
    else
        print_warning "Valgrind not available, skipping detailed memory analysis"
    fi
    
    cd ../..
    echo
}

benchmark_performance() {
    print_header "Performance Benchmarking"
    
    cd "$BUILD_DIR/tests"
    
    # Run comprehensive benchmarks
    echo "Running performance benchmarks..."
    ./"$TEST_EXECUTABLE" --gtest_filter="*Benchmark*" | tee "../$PERFORMANCE_LOG"
    
    # Extract key performance metrics
    echo
    echo "Key Performance Metrics:"
    echo "========================"
    
    if grep -q "BENCHMARK" "../../$PERFORMANCE_LOG"; then
        grep "BENCHMARK" "../../$PERFORMANCE_LOG" | while read -r line; do
            echo "  $line"
        done
    else
        print_warning "No benchmark results found in log"
    fi
    
    cd ../..
    echo
}

generate_report() {
    print_header "Generating Performance Report"
    
    REPORT_FILE="performance_report_$(date +%Y%m%d_%H%M%S).md"
    
    cat > "$REPORT_FILE" << EOF
# Opera Chess Engine Performance Report

**Generated:** $(date)
**System:** $(uname -a)
**Compiler:** $(c++ --version | head -n1)

## Test Results Summary

EOF
    
    cd "$BUILD_DIR/tests"
    
    # Check if XML results exist and parse them
    for xml_file in *.xml; do
        if [ -f "$xml_file" ]; then
            echo "### $(basename "$xml_file" .xml)" >> "../$REPORT_FILE"
            
            # Extract test results (simplified parsing)
            tests=$(grep -c "<testcase" "$xml_file" 2>/dev/null || echo "0")
            failures=$(grep -c "<failure" "$xml_file" 2>/dev/null || echo "0")
            
            echo "- Tests run: $tests" >> "../$REPORT_FILE"
            echo "- Failures: $failures" >> "../$REPORT_FILE"
            echo >> "../$REPORT_FILE"
        fi
    done
    
    cd ..
    
    # Add performance metrics to report
    if [ -f "$PERFORMANCE_LOG" ]; then
        echo "## Performance Metrics" >> "$REPORT_FILE"
        echo >> "$REPORT_FILE"
        echo "\`\`\`" >> "$REPORT_FILE"
        grep "BENCHMARK\|MEMORY AUDIT" "$PERFORMANCE_LOG" >> "$REPORT_FILE"
        echo "\`\`\`" >> "$REPORT_FILE"
        echo >> "$REPORT_FILE"
    fi
    
    # Add system information
    echo "## System Information" >> "$REPORT_FILE"
    echo >> "$REPORT_FILE"
    echo "- **CPU:** $(sysctl -n machdep.cpu.brand_string 2>/dev/null || grep 'model name' /proc/cpuinfo | head -n1 | cut -d: -f2 | xargs)" >> "$REPORT_FILE"
    echo "- **Memory:** $(free -h 2>/dev/null | grep 'Mem:' | awk '{print $2}' || sysctl -n hw.memsize | awk '{print $1/1024/1024/1024 " GB"}')" >> "$REPORT_FILE"
    echo "- **Build Type:** $(cmake --build "$BUILD_DIR" --config Release &>/dev/null && echo "Release" || echo "Debug")" >> "$REPORT_FILE"
    
    print_success "Performance report generated: $REPORT_FILE"
    echo
}

cleanup() {
    print_header "Cleanup"
    
    # Archive old log files
    if [ -f "$PERFORMANCE_LOG" ]; then
        mv "$PERFORMANCE_LOG" "logs/$(basename "$PERFORMANCE_LOG" .log)_$(date +%Y%m%d_%H%M%S).log" 2>/dev/null || true
    fi
    
    # Clean up temporary files in build directory
    cd "$BUILD_DIR/tests" 2>/dev/null || true
    rm -f *.xml valgrind_output.log 2>/dev/null || true
    cd - > /dev/null
    
    print_success "Cleanup completed"
}

main() {
    echo "Starting performance test suite..."
    echo
    
    # Create logs directory if it doesn't exist
    mkdir -p logs
    
    # Run test suite
    check_prerequisites
    run_performance_tests
    run_stress_tests
    check_memory_usage
    benchmark_performance
    generate_report
    cleanup
    
    echo
    print_success "Performance test suite completed successfully!"
    echo
    echo "Next steps:"
    echo "  1. Review the generated performance report"
    echo "  2. Compare results with previous benchmarks"
    echo "  3. Investigate any performance regressions"
    echo "  4. Update performance baselines if improvements are confirmed"
}

# Handle script arguments
case "${1:-}" in
    --quick)
        echo "Running quick performance tests only..."
        check_prerequisites
        run_performance_tests
        ;;
    --memory-only)
        echo "Running memory audit tests only..."
        check_prerequisites
        cd "$BUILD_DIR/tests"
        ./"$TEST_EXECUTABLE" --gtest_filter="MemoryAuditTest.*"
        cd ..
        ;;
    --benchmark-only)
        echo "Running benchmark tests only..."
        check_prerequisites
        benchmark_performance
        ;;
    --help|-h)
        echo "Usage: $0 [OPTIONS]"
        echo
        echo "OPTIONS:"
        echo "  --quick         Run only basic performance tests"
        echo "  --memory-only   Run only memory audit tests"
        echo "  --benchmark-only Run only benchmark tests"
        echo "  --help          Show this help message"
        echo
        exit 0
        ;;
    *)
        main
        ;;
esac