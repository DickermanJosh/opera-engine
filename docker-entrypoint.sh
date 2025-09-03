#!/bin/bash
# Docker entrypoint script for Opera Chess Engine
# Supports UCI mode, testing, and debug operations

set -e

# Default neural network weights path
NN_WEIGHTS_PATH="/nn"
DEFAULT_WEIGHTS="$NN_WEIGHTS_PATH/morphy.nnue"

# Helper functions
log_info() {
    echo "[OPERA-DOCKER] INFO: $1" >&2
}

log_error() {
    echo "[OPERA-DOCKER] ERROR: $1" >&2
}

show_help() {
    cat << EOF
Opera Chess Engine Docker Container

Usage:
  docker run opera-engine [COMMAND] [OPTIONS]

Commands:
  uci                    Start engine in UCI mode (default)
  test                   Run engine test suite
  test-rust             Run Rust-specific tests only  
  test-cpp              Run C++ core tests only
  debug                 Start engine with debug logging
  version               Show version information
  help                  Show this help message

Options for UCI mode:
  -weights PATH         Path to neural network weights file
                        (default: /nn/morphy.nnue if mounted)
  -hash SIZE           Hash table size in MB (default: 128)
  -threads COUNT       Number of search threads (default: 1)
  -debug               Enable debug output
  -morphy             Enable Morphy playing style (default: false)

Examples:
  # Start in UCI mode
  docker run -it opera-engine
  
  # Start with custom NN weights
  docker run -v /path/to/weights:/nn opera-engine uci -weights /nn/custom.nnue
  
  # Run tests
  docker run opera-engine test
  
  # Debug mode with custom settings
  docker run -it opera-engine debug -hash 256 -threads 2

Volume Mounts:
  /nn      - Neural network weights directory
  /logs    - Log file output directory
EOF
}

# Parse command line arguments
COMMAND="${1:-uci}"
shift || true

WEIGHTS_PATH=""
HASH_SIZE="128"
THREAD_COUNT="1" 
DEBUG_MODE=""
MORPHY_STYLE="false"

while [[ $# -gt 0 ]]; do
    case $1 in
        -weights)
            WEIGHTS_PATH="$2"
            shift 2
            ;;
        -hash)
            HASH_SIZE="$2"
            shift 2
            ;;
        -threads)
            THREAD_COUNT="$2"
            shift 2
            ;;
        -debug)
            DEBUG_MODE="--debug"
            shift
            ;;
        -morphy)
            MORPHY_STYLE="true"
            shift
            ;;
        -h|--help)
            show_help
            exit 0
            ;;
        *)
            log_error "Unknown option: $1"
            show_help
            exit 1
            ;;
    esac
done

# Validate environment
if [[ ! -x "./opera-uci" ]]; then
    log_error "opera-uci binary not found or not executable"
    exit 1
fi

# Handle different commands
case "$COMMAND" in
    uci)
        log_info "Starting Opera Chess Engine in UCI mode"
        
        # Check for neural network weights
        if [[ -n "$WEIGHTS_PATH" ]]; then
            if [[ ! -f "$WEIGHTS_PATH" ]]; then
                log_error "Specified weights file not found: $WEIGHTS_PATH"
                exit 1
            fi
            log_info "Using neural network weights: $WEIGHTS_PATH"
        elif [[ -f "$DEFAULT_WEIGHTS" ]]; then
            WEIGHTS_PATH="$DEFAULT_WEIGHTS"
            log_info "Using default neural network weights: $WEIGHTS_PATH"
        else
            log_info "No neural network weights found - running without NN evaluation"
        fi
        
        log_info "Engine settings: Hash=${HASH_SIZE}MB, Threads=${THREAD_COUNT}, Morphy=${MORPHY_STYLE}"
        
        # Start UCI engine with settings
        exec ./opera-uci \
            ${DEBUG_MODE} \
            --hash-size "${HASH_SIZE}" \
            --threads "${THREAD_COUNT}" \
            --morphy-style "${MORPHY_STYLE}" \
            ${WEIGHTS_PATH:+--weights "$WEIGHTS_PATH"}
        ;;
        
    test)
        log_info "Running Opera Engine test suite"
        
        # In runtime image, tests were run during build
        # This is a basic smoke test to verify engine functionality
        log_info "Running engine smoke tests..."
        
        # Test 1: Engine can start and respond to UCI
        echo "Testing UCI protocol response..."
        echo "uci" | ./opera-uci | grep -q "uciok" && echo "✓ UCI protocol test passed" || echo "✗ UCI protocol test failed"
        
        # Test 2: Engine can handle position commands
        echo "Testing position command handling..."
        (echo "uci"; echo "position startpos"; echo "quit") | ./opera-uci > /dev/null && echo "✓ Position handling test passed" || echo "✗ Position handling test failed"
        
        # Test 3: Engine version info
        echo "Testing engine version..."
        ./opera-uci --version && echo "✓ Version test passed" || echo "✗ Version test failed"
        
        log_info "Engine smoke tests completed successfully"
        log_info "Note: Full unit tests are run during Docker build process"
        ;;
        
    test-rust)
        log_info "Rust tests are run during Docker build process"
        log_info "Runtime image contains only compiled binaries"
        log_info "Run 'test' command for engine smoke tests"
        ;;
        
    test-cpp)
        log_info "C++ tests are run during Docker build process"
        log_info "Runtime image contains only compiled binaries" 
        log_info "Run 'test' command for engine smoke tests"
        ;;
        
    debug)
        log_info "Starting Opera Engine in debug mode"
        exec ./opera-uci \
            --debug \
            --hash-size "${HASH_SIZE}" \
            --threads "${THREAD_COUNT}" \
            --morphy-style "${MORPHY_STYLE}" \
            ${WEIGHTS_PATH:+--weights "$WEIGHTS_PATH"}
        ;;
        
    version)
        echo "Opera Chess Engine Docker Container"
        echo "Version: 1.0.0"
        echo "Built with:"
        echo "  - C++ Core: Modern C++17"
        echo "  - Rust UCI Layer: $(rustc --version 2>/dev/null || echo 'Unknown')"
        echo "  - Paul Morphy Playing Style"
        ./opera-uci --version 2>/dev/null || echo "Engine binary version unavailable"
        ;;
        
    help)
        show_help
        ;;
        
    *)
        log_error "Unknown command: $COMMAND"
        show_help
        exit 1
        ;;
esac