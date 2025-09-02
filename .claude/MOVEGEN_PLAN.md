# Move Generation Implementation Plan

## Overview
Implement comprehensive move generation for the Opera chess engine with extensive testing coverage, focusing on correctness, performance, and edge case handling.

## Phase 1: Core Move Representation

### 1.1 Move Structure Design
```cpp
class Move {
private:
    uint32_t data;  // Packed representation
    
    // Bit layout (32 bits total):
    // Bits 0-5:   From square (6 bits, 0-63)
    // Bits 6-11:  To square (6 bits, 0-63)  
    // Bits 12-14: Move type (3 bits)
    // Bits 15-17: Promotion piece (3 bits)
    // Bits 18-23: Captured piece (6 bits)
    // Bits 24-31: Reserved/flags (8 bits)
    
public:
    enum MoveType {
        NORMAL = 0,
        CASTLING = 1,
        EN_PASSANT = 2,
        PROMOTION = 3,
        DOUBLE_PAWN_PUSH = 4
    };
};
```

### 1.2 Tests for Move Representation
**Test File**: `tests/test_move.cpp`

#### Positive Tests:
- **Construction**: Valid moves with all combinations of from/to squares
- **Packing/Unpacking**: Bit manipulation correctness
- **Move Types**: All special move types correctly identified
- **Promotion**: All promotion pieces (Q, R, B, N) correctly stored
- **Capture Detection**: Captured piece type correctly stored
- **Equality**: Move comparison operators work correctly

#### Negative Tests:
- **Invalid Squares**: Squares > 63 should throw or assert
- **Invalid Piece Types**: Invalid promotion pieces rejected
- **Null Moves**: Handle null/invalid move construction
- **Corruption**: Verify data integrity after manipulation

#### Edge Case Tests:
- **Maximum Values**: All bit fields at maximum values
- **Boundary Conditions**: Square 0, square 63, all corners
- **Special Combinations**: Castling + promotion (impossible but test anyway)

## Phase 2: Move List Implementation

### 2.1 Move List Design
```cpp
template<size_t MAX_MOVES = 256>
class MoveList {
private:
    Move moves[MAX_MOVES];
    size_t count;
    
public:
    void add(Move move);
    void clear();
    size_t size() const;
    Move& operator[](size_t index);
    iterator begin();
    iterator end();
};
```

### 2.2 Tests for Move List
**Test File**: `tests/test_movelist.cpp`

#### Positive Tests:
- **Basic Operations**: Add, clear, size, access
- **Iterator Support**: Range-based for loops work
- **Capacity**: Fill to maximum capacity successfully
- **Copy/Move Semantics**: Proper copy constructor and assignment
- **Sorting**: Move ordering and sorting functionality

#### Negative Tests:
- **Overflow**: Adding beyond capacity handled gracefully
- **Out of Bounds**: Array access bounds checking
- **Empty List**: Operations on empty list don't crash

#### Edge Case Tests:
- **Single Move**: List with exactly one move
- **Full Capacity**: List at maximum capacity
- **Repeated Adds**: Same move added multiple times

## Phase 3: Piece Move Generation

### 3.1 Magic Bitboard Implementation
```cpp
class MagicBitboards {
private:
    static uint64_t rookMagics[64];
    static uint64_t bishopMagics[64];
    static uint64_t rookMasks[64];
    static uint64_t bishopMasks[64];
    static uint64_t* rookAttacks[64];
    static uint64_t* bishopAttacks[64];
    
public:
    static void init();
    static uint64_t getRookAttacks(Square sq, uint64_t occupied);
    static uint64_t getBishopAttacks(Square sq, uint64_t occupied);
};
```

### 3.2 Individual Piece Move Generation

#### 3.2.1 Pawn Moves
**Implementation Priority**: Highest (most complex rules)
```cpp
class PawnMoveGenerator {
public:
    void generatePawnMoves(const Board& board, MoveList& moves, Color color);
    void generatePawnCaptures(const Board& board, MoveList& moves, Color color);
    void generateEnPassant(const Board& board, MoveList& moves, Color color);
    void generatePromotions(const Board& board, MoveList& moves, Color color);
};
```

**Tests**: `tests/test_pawn_moves.cpp`
- **Single Push**: One square forward, blocked/unblocked
- **Double Push**: Two squares from starting rank
- **Captures**: Diagonal captures left/right
- **En Passant**: All en passant scenarios
- **Promotions**: All promotion pieces on 7th/2nd rank
- **Promotion Captures**: Capturing promotions
- **Edge Cases**: Pawns on edges, corners, blocked paths

#### 3.2.2 Knight Moves
**Tests**: `tests/test_knight_moves.cpp`
- **Center Board**: All 8 moves from central squares
- **Edge Squares**: Reduced moves from edges
- **Corner Squares**: Minimum moves from corners
- **Blocked Squares**: Own pieces blocking destinations
- **Captures**: Enemy pieces on destination squares

#### 3.2.3 Bishop Moves
**Tests**: `tests/test_bishop_moves.cpp`
- **Open Diagonals**: Full diagonal movement
- **Blocked Diagonals**: Own/enemy pieces blocking
- **Corner to Corner**: Maximum distance moves
- **Single Square**: Minimum distance moves
- **Magic Bitboard Verification**: Compare with slow generation

#### 3.2.4 Rook Moves
**Tests**: `tests/test_rook_moves.cpp`
- **Open Ranks/Files**: Full rank/file movement
- **Blocked Paths**: Pieces blocking horizontal/vertical
- **Edge Cases**: From/to edges and corners
- **Magic Bitboard Verification**: Compare with slow generation

#### 3.2.5 Queen Moves
**Tests**: `tests/test_queen_moves.cpp`
- **Combined Movement**: Rook + Bishop moves
- **Maximum Mobility**: From center positions
- **Minimum Mobility**: From corner positions
- **Complex Blocking**: Multiple pieces blocking different directions

#### 3.2.6 King Moves
**Tests**: `tests/test_king_moves.cpp`
- **Normal Moves**: All 8 adjacent squares
- **Castling**: Kingside and queenside castling
- **Castling Blocked**: Pieces blocking castling path
- **Castling Under Check**: Cannot castle through check
- **Castling Rights**: Proper castling rights management
- **King Safety**: Moves into check detection

## Phase 4: Complete Move Generation

### 4.1 Main Move Generator
```cpp
class MoveGenerator {
public:
    void generateAllMoves(const Board& board, MoveList& moves);
    void generateCaptures(const Board& board, MoveList& moves);
    void generateQuietMoves(const Board& board, MoveList& moves);
    void generateLegalMoves(const Board& board, MoveList& moves);
    bool isLegal(const Board& board, Move move);
    bool isInCheck(const Board& board, Color color);
};
```

### 4.2 Tests for Complete Move Generation
**Test File**: `tests/test_move_generator.cpp`

#### Comprehensive Position Tests:
1. **Starting Position**: Exactly 20 legal moves for white
2. **Complex Middle Game**: Known positions with verified move counts
3. **Endgame Positions**: King and pawn endings
4. **Tactical Positions**: Pins, forks, skewers affecting legal moves
5. **Check Positions**: Limited legal moves when in check
6. **Checkmate Positions**: Zero legal moves
7. **Stalemate Positions**: No legal moves but not in check

## Phase 5: Legal Move Validation

### 5.1 Legal Move Checker
```cpp
class LegalityChecker {
public:
    bool isMoveLegal(const Board& board, Move move);
    bool wouldBeInCheck(const Board& board, Move move, Color color);
    bool isPinned(const Board& board, Square square, Color color);
    uint64_t getPinnedPieces(const Board& board, Color color);
};
```

### 5.2 Tests for Legal Move Validation
**Test File**: `tests/test_legality.cpp`

#### Pin Detection Tests:
- **Horizontal Pins**: Rook/Queen pinning on ranks
- **Vertical Pins**: Rook/Queen pinning on files  
- **Diagonal Pins**: Bishop/Queen pinning on diagonals
- **Multiple Pins**: Several pieces pinned simultaneously
- **Absolute Pins**: Piece cannot move at all
- **Relative Pins**: Piece can move along pin line

#### Check Detection Tests:
- **Direct Attacks**: Each piece type giving check
- **Discovered Check**: Moving piece reveals check
- **Double Check**: Two pieces giving check simultaneously
- **Check Evasion**: All legal ways to escape check

## Phase 6: Performance Move Generation

### 6.1 Optimized Generators
```cpp
class FastMoveGenerator {
public:
    void generateMovesOptimized(const Board& board, MoveList& moves);
    void generateCapturesOnly(const Board& board, MoveList& moves);
    void generateChecksOnly(const Board& board, MoveList& moves);
    void generateQuietMovesOnly(const Board& board, MoveList& moves);
};
```

### 6.2 Performance Tests
**Test File**: `tests/test_move_performance.cpp`

#### Benchmarks:
- **Speed Tests**: Moves per second generation
- **Memory Usage**: Stack/heap usage during generation  
- **Cache Performance**: Memory access patterns
- **Scaling**: Performance across different position types

## Phase 7: Perft Testing Framework

### 7.1 Perft Implementation
```cpp
class PerftTester {
public:
    uint64_t perft(Board& board, int depth);
    uint64_t perftDivide(Board& board, int depth);
    void runPerftSuite();
    bool validatePerftPosition(const std::string& fen, int depth, uint64_t expected);
};
```

### 7.2 Perft Test Suite
**Test File**: `tests/test_perft.cpp`

#### Standard Perft Positions:
1. **Starting Position**:
   - Depth 1: 20 moves
   - Depth 2: 400 moves  
   - Depth 3: 8,902 moves
   - Depth 4: 197,281 moves
   - Depth 5: 4,865,609 moves
   - Depth 6: 119,060,324 moves

2. **Kiwipete Position**:
   ```
   r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1
   ```
   - Depth 1: 48 moves
   - Depth 2: 2,039 moves
   - Depth 3: 97,862 moves
   - Depth 4: 4,085,603 moves

3. **Position 3**: 
   ```
   8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1
   ```

4. **Position 4**:
   ```
   r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1
   ```

5. **Position 5**:
   ```
   rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8
   ```

6. **Your Custom Perft Suite**: All 20 positions from your dataset with depth 9 verification

### 7.3 Extended Perft Testing
```cpp
// Additional perft verification methods
class ExtendedPerftTester {
public:
    struct PerftResult {
        uint64_t nodes;
        uint64_t captures;
        uint64_t enPassant;
        uint64_t castles;
        uint64_t promotions;
        uint64_t checks;
        uint64_t checkmates;
    };
    
    PerftResult detailedPerft(Board& board, int depth);
    bool compareDetailedResults(const PerftResult& expected, const PerftResult& actual);
};
```

## Phase 8: Regression Testing

### 8.1 Regression Test Suite
**Test File**: `tests/test_regression.cpp`

#### Historical Bug Prevention:
- **Known Chess Bugs**: En passant edge cases, castling rights
- **Implementation Bugs**: Off-by-one errors, bit manipulation bugs  
- **Performance Regressions**: Ensure optimizations don't break correctness
- **Memory Issues**: Valgrind/AddressSanitizer clean runs

### 8.2 Continuous Integration Tests
```bash
# Test script: run_move_tests.sh
#!/bin/bash

echo "Running move generation test suite..."

# Unit tests for each component
./test_move
./test_movelist  
./test_pawn_moves
./test_knight_moves
./test_bishop_moves
./test_rook_moves
./test_queen_moves
./test_king_moves
./test_move_generator
./test_legality
./test_performance

# Perft verification
echo "Running Perft tests..."
./test_perft

# Regression tests
echo "Running regression tests..."
./test_regression

echo "All tests completed!"
```

## Phase 9: Integration Testing

### 9.1 Board Integration Tests
**Test File**: `tests/test_board_integration.cpp`

#### Make/Unmake Testing:
- **Reversibility**: Make move, unmake move returns to original
- **Hash Consistency**: Zobrist key correctly updated/restored
- **State Preservation**: All board state correctly maintained
- **Multiple Moves**: Chain of moves and unmoves
- **Special Moves**: Castling, en passant, promotion reversibility

### 9.2 UCI Integration Tests
**Test File**: `tests/test_uci_integration.cpp`

#### Move Format Testing:
- **Algebraic Notation**: Conversion to/from UCI format
- **Move Parsing**: Parse UCI moves correctly
- **Move Validation**: Reject invalid UCI moves
- **Special Move Format**: Castling (e1g1), promotion (e7e8q)

## Phase 10: Stress Testing

### 10.1 Stress Test Suite
**Test File**: `tests/test_stress.cpp`

#### Endurance Tests:
- **Million Move Generation**: Generate moves for 1M random positions
- **Deep Perft**: Run perft to depth 7+ on complex positions
- **Memory Stress**: Generate moves without memory leaks
- **Random Position Testing**: Generate and validate moves for random positions

#### Fuzzing Tests:
- **Random FEN**: Generate moves for randomly created (valid) FEN positions
- **Boundary Conditions**: Test with positions at edge of validity
- **Malformed Input**: Graceful handling of invalid positions

## Testing Infrastructure

### Test Organization
```
tests/
├── unit/                   # Individual component tests
│   ├── test_move.cpp
│   ├── test_movelist.cpp
│   └── test_*_moves.cpp
├── integration/            # Component integration tests
│   ├── test_board_integration.cpp
│   └── test_uci_integration.cpp
├── performance/            # Performance benchmarks
│   ├── test_move_performance.cpp
│   └── benchmark_perft.cpp
├── regression/             # Regression test suite
│   └── test_regression.cpp
├── perft/                  # Perft testing
│   ├── test_perft.cpp
│   ├── perft_positions.txt
│   └── custom_perft_suite.txt  # Your 20 positions
└── stress/                 # Stress and fuzz testing
    ├── test_stress.cpp
    └── test_fuzzing.cpp
```

### Test Data Management
```
tests/data/
├── standard_positions.fen  # Well-known test positions
├── tactical_positions.fen  # Tactical puzzle positions
├── endgame_positions.fen   # Endgame test positions
├── perft_suite.txt         # Complete perft test suite
└── regression_cases.txt    # Historical bug cases
```

### Automated Testing
```cmake
# CMakeLists.txt test configuration
enable_testing()

# Add all test executables
add_test(NAME move_tests COMMAND test_move)
add_test(NAME movelist_tests COMMAND test_movelist)
add_test(NAME pawn_tests COMMAND test_pawn_moves)
add_test(NAME knight_tests COMMAND test_knight_moves)
add_test(NAME bishop_tests COMMAND test_bishop_moves)
add_test(NAME rook_tests COMMAND test_rook_moves)
add_test(NAME queen_tests COMMAND test_queen_moves)
add_test(NAME king_tests COMMAND test_king_moves)
add_test(NAME generator_tests COMMAND test_move_generator)
add_test(NAME legality_tests COMMAND test_legality)
add_test(NAME perft_tests COMMAND test_perft)
add_test(NAME integration_tests COMMAND test_board_integration)
add_test(NAME regression_tests COMMAND test_regression)
add_test(NAME stress_tests COMMAND test_stress)

# Performance tests (optional, longer running)
add_test(NAME performance_tests COMMAND test_move_performance)
```

## Success Criteria

### Correctness Requirements:
- **100% Test Coverage**: Every function has comprehensive tests
- **All Perft Tests Pass**: Including your custom 20-position suite at depth 9
- **Zero Regression Failures**: All historical bug cases prevented
- **Legal Move Accuracy**: 100% accurate legal move detection
- **Special Move Handling**: Perfect castling, en passant, promotion logic

### Performance Requirements:
- **Move Generation Speed**: > 10M moves/second for typical positions
- **Perft Performance**: Depth 6 perft in < 1 second for starting position
- **Memory Efficiency**: Minimal allocation during move generation
- **Scalability**: Performance degrades gracefully with position complexity

### Quality Requirements:
- **Clean Valgrind Run**: No memory errors or leaks
- **Warning-Free Compilation**: -Wall -Wextra -Wpedantic clean
- **Documentation Coverage**: All public APIs documented
- **Code Review Ready**: Clean, readable, maintainable code

## Implementation Timeline

1. **Week 1**: Move representation and basic tests
2. **Week 2**: Individual piece move generation with full test coverage
3. **Week 3**: Complete move generator and legal move validation
4. **Week 4**: Perft implementation and standard test suite
5. **Week 5**: Custom perft suite (your 20 positions) and optimization
6. **Week 6**: Integration testing and UCI compatibility
7. **Week 7**: Performance optimization and stress testing
8. **Week 8**: Final validation and regression testing

Remember: **Tests first, implementation second**. Write comprehensive tests before implementing each component. This ensures we catch edge cases early and maintain code quality throughout development.