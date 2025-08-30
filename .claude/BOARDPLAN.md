# Opera Engine Board State & Entry Point Plan

## Overview
This document outlines the implementation strategy for the board state representation and entry point architecture for the Opera Chess Engine, based on the specifications in CLAUDE.md.

## Board State Design Decisions

### Core Bitboard Architecture
The board will use a 64-bit integer bitboard representation for maximum performance:

```cpp
class Board {
private:
    // Bitboard representation - one bitboard per piece type per color
    uint64_t pieces[12];  // [WHITE_PAWN, WHITE_KNIGHT, ..., BLACK_KING]
    uint64_t occupied[3]; // [WHITE_PIECES, BLACK_PIECES, ALL_PIECES]
    
    // Game state tracking
    CastlingRights castling;  // 4 bits: KQkq
    Square enPassant;         // En passant target square
    int halfmoveClock;        // 50-move rule counter
    int fullmoveNumber;       // Game move counter
    Color sideToMove;         // WHITE or BLACK
    
    // Zobrist hashing for transposition tables
    uint64_t zobristKey;
    
    // Move history for undoing moves
    std::vector<BoardState> history;
};
```

### Piece Representation Strategy
- **12 Bitboards**: One for each piece type and color combination
- **3 Occupancy Bitboards**: White pieces, black pieces, and all pieces for fast collision detection
- **Magic Bitboards**: Pre-computed lookup tables for sliding pieces (rooks, bishops, queens)

### Memory Layout Optimization
- Structure members ordered for optimal cache alignment
- Bitboards grouped together for better cache locality
- Game state variables packed efficiently

## Entry Point Architecture

### Main Application Structure
```cpp
// main.cpp - Primary entry point
int main(int argc, char* argv[]) {
    try {
        // Initialize logging system
        Logger::initialize(LogLevel::INFO);
        
        // Parse command line arguments
        CommandLineArgs args = parseArguments(argc, argv);
        
        // Initialize engine core components
        Engine engine(args.configFile);
        
        // Determine execution mode
        if (args.mode == Mode::UCI) {
            UCIInterface uci(engine);
            return uci.run();
        } else if (args.mode == Mode::BENCHMARK) {
            Benchmark bench(engine);
            return bench.run();
        } else if (args.mode == Mode::PERFT) {
            PerftTest perft(engine);
            return perft.run(args.depth);
        }
        
    } catch (const std::exception& e) {
        Logger::error("Fatal error: {}", e.what());
        return 1;
    }
    
    return 0;
}
```

### Execution Modes
1. **UCI Mode**: Standard UCI protocol interface
2. **Benchmark Mode**: Performance testing and evaluation
3. **Perft Mode**: Move generation verification
4. **Analysis Mode**: Position analysis utilities

## Core Component Integration

### Engine Class Design
```cpp
class Engine {
private:
    Board board;
    MoveGenerator moveGen;
    Search search;
    Evaluator evaluator;
    TranspositionTable transTable;
    
public:
    Engine(const std::string& configFile = "");
    
    // Core engine operations
    void newGame();
    void setPosition(const std::string& fen);
    void setPosition(const std::string& startpos, const std::vector<std::string>& moves);
    
    Move search(int depth, int timeMs = 0);
    bool makeMove(const Move& move);
    void undoMove();
    
    // Configuration
    void setOption(const std::string& name, const std::string& value);
    std::vector<std::string> getOptions() const;
};
```

### Move Representation
```cpp
struct Move {
    Square from : 6;     // Source square (0-63)
    Square to : 6;       // Destination square (0-63)
    PieceType piece : 3; // Moving piece type
    PieceType captured : 3; // Captured piece type (NONE if no capture)
    MoveType type : 4;   // NORMAL, CASTLE, EN_PASSANT, PROMOTION
    PieceType promotion : 3; // Promotion piece type
    
    // Utility methods
    bool isCapture() const { return captured != NONE; }
    bool isPromotion() const { return type == PROMOTION; }
    bool isCastling() const { return type == CASTLE; }
    std::string toString() const; // Algebraic notation
};
```

## Performance Considerations

### Magic Bitboard Implementation
- Pre-computed magic numbers for rook and bishop attacks
- Compact lookup tables using perfect hashing
- Runtime generation vs. pre-computed tables trade-off

### Memory Management
- Stack-based allocation for search tree
- Minimal heap allocations during search
- Transposition table with configurable size

### Threading Strategy
- Main search thread with helper threads
- Lock-free data structures where possible
- NUMA-aware memory allocation on supported systems

## Morphy Style Integration Points

### Board Evaluation Hooks
```cpp
class MorphyEvaluator : public Evaluator {
    int evaluate(const Board& board) override {
        int materialScore = evaluateMaterial(board);
        int positionalScore = evaluatePosition(board);
        
        // Morphy-specific adjustments
        int developmentBonus = evaluateDevelopment(board) * MORPHY_DEVELOPMENT_WEIGHT;
        int initiativeBonus = evaluateInitiative(board) * MORPHY_INITIATIVE_WEIGHT;
        int kingSafetyThreat = evaluateKingThreat(board) * MORPHY_KING_HUNT_WEIGHT;
        
        return materialScore + positionalScore + developmentBonus + 
               initiativeBonus + kingSafetyThreat;
    }
};
```

### Search Modifications
- Extended search for sacrificial combinations
- Reduced depth for passive moves
- Priority given to forcing moves and checks

## Configuration System

### Engine Options
```yaml
# opera-engine.yaml
engine:
  hash_size: 256        # MB
  threads: 1
  morphy_style: true
  
morphy_settings:
  development_weight: 1.2
  initiative_weight: 1.1  
  king_hunt_weight: 1.5
  sacrifice_threshold: 100  # centipawns
  
debug:
  log_level: info
  perft_verification: true
  search_debug: false
```

### Runtime Configuration
- Command-line option overrides
- UCI option support for GUI integration
- Dynamic reconfiguration during gameplay

## Testing Strategy

### Unit Tests
- Board state consistency tests
- Move generation verification (perft)
- Zobrist key collision detection
- FEN parsing/generation roundtrip tests

### Integration Tests
- UCI protocol compliance
- Search algorithm correctness
- Evaluation function consistency
- Performance benchmarks

### Morphy Style Validation
- Tactical puzzle solving
- Style consistency metrics
- Historical game recreation tests
- Sacrifice detection accuracy

## Build System Integration

### CMake Configuration
```cmake
cmake_minimum_required(VERSION 3.16)
project(OperaEngine VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Core engine library
add_library(opera_core STATIC
    src/board/Board.cpp
    src/board/MoveGenerator.cpp
    src/search/Search.cpp
    src/evaluation/Evaluator.cpp
    src/uci/UCIInterface.cpp
)

# Main executable
add_executable(opera-engine src/main.cpp)
target_link_libraries(opera-engine opera_core)

# Python bindings
find_package(pybind11 REQUIRED)
pybind11_add_module(opera_python src/python/bindings.cpp)
target_link_libraries(opera_python PRIVATE opera_core)
```

### Dependencies
- **Standard Library**: C++17 features (std::optional, std::variant)
- **pybind11**: Python integration
- **yaml-cpp**: Configuration parsing
- **Google Test**: Unit testing framework
- **Google Benchmark**: Performance testing

## Directory Structure Implementation

```
cpp/
├── src/
│   ├── board/
│   │   ├── Board.h/.cpp          # Main board representation
│   │   ├── MoveGenerator.h/.cpp  # Move generation
│   │   ├── MagicBitboards.h/.cpp # Magic bitboard tables
│   │   └── Zobrist.h/.cpp        # Zobrist hashing
│   ├── search/
│   │   ├── Search.h/.cpp         # Main search algorithm
│   │   ├── TranspositionTable.h/.cpp
│   │   └── MorphySearch.h/.cpp   # Style-specific search
│   ├── evaluation/
│   │   ├── Evaluator.h/.cpp      # Base evaluator
│   │   └── MorphyEvaluator.h/.cpp # Style-specific evaluation
│   ├── uci/
│   │   └── UCIInterface.h/.cpp   # UCI protocol
│   ├── utils/
│   │   ├── Logger.h/.cpp         # Logging system
│   │   ├── Config.h/.cpp         # Configuration management
│   │   └── Types.h               # Common type definitions
│   └── main.cpp                  # Application entry point
├── include/                      # Public headers
├── tests/                        # Unit tests
└── CMakeLists.txt
```

## Next Implementation Steps

1. **Foundation**: Implement basic Board class with bitboard representation
2. **Move Generation**: Create MoveGenerator with magic bitboards
3. **UCI Interface**: Basic UCI protocol for testing with existing GUIs
4. **Search Framework**: Alpha-beta search with transposition table
5. **Evaluation**: Basic material + positional evaluation
6. **Morphy Integration**: Style-specific modifications to search and evaluation
7. **Python Bindings**: pybind11 wrapper for AI integration
8. **Testing Suite**: Comprehensive test coverage
9. **Performance Optimization**: Profile-guided optimization
10. **Platform Integration**: Lichess and custom app connectors

## Success Metrics

### Technical Benchmarks
- **Perft Verification**: Match known perft results for all depths
- **Search Speed**: > 1M nodes/second on modern hardware
- **Memory Efficiency**: Configurable hash usage without leaks
- **UCI Compliance**: Pass all standard UCI test suites

### Style Validation
- **Tactical Rating**: > 2000 ELO on tactical puzzle sets
- **Sacrifice Detection**: Identify sound sacrifices in test positions
- **Development Speed**: Prioritize development in opening positions
- **King Hunt**: Demonstrate aggressive king attack patterns

This board plan provides a comprehensive roadmap for implementing the core chess engine with Morphy-style characteristics while maintaining high performance and clean architecture principles.