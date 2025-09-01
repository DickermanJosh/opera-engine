# Debug Files Directory

This directory contains debugging and development helper programs used during the implementation phases of the Opera Chess Engine.

## File Categories

### Move Generation Debug Programs
- `debug_pawn.cpp` - Debug pawn move generation
- `debug_knight.cpp` - Debug knight move generation  
- `debug_sliding.cpp` - Debug sliding piece move generation
- `debug_endgame.cpp` - Debug endgame scenarios
- `debug_promotion.cpp` - Debug pawn promotion moves

### Board and Position Analysis
- `debug_test.cpp` - General board testing and validation
- `square_debug.cpp` - Square indexing and conversion debugging
- `debug_starting_position.cpp` - Debug starting position move generation
- `debug_test_position.cpp` - Debug specific test positions

### Attack Generation Analysis
- `debug_attack_gen.cpp` - Debug attack bitboard generation
- `debug_multiple_bishops.cpp` - Debug multiple piece interactions
- `debug_fix_test.cpp` - Debug test position corrections

### Sliding Piece Specific Debugging  
- `debug_rook_blocked.cpp` - Debug rook blocking scenarios
- `debug_rook_captures.cpp` - Debug rook capture moves

### Performance Testing
- `run_performance_tests.sh` - Performance benchmark script

## How to Build and Run

These debug programs are not built by default with the main project. To compile and run a debug program:

```bash
# From the cpp directory
cd /path/to/opera-engine/cpp

# Compile a debug program (example with debug_pawn.cpp)
g++ -std=c++17 -I include -I src/board \
    debug/debug_pawn.cpp \
    src/board/Board.cpp \
    src/board/MoveGenerator.cpp \
    src/utils/Types.cpp \
    -o debug_pawn

# Run the compiled debug program
./debug_pawn
```

## Purpose

These files were instrumental in:
- **Test-Driven Development**: Creating failing tests first, then implementing features
- **Bug Investigation**: Isolating and reproducing specific issues
- **Position Analysis**: Understanding complex chess positions and move generation
- **Performance Debugging**: Analyzing timing and memory usage
- **FEN Validation**: Ensuring test positions were correctly specified

## Clean-up Status

All debug files have been moved here to keep the main source directories clean and organized. The main project builds without these files and maintains 100% test coverage through the formal unit test suite in the `/tests` directory.