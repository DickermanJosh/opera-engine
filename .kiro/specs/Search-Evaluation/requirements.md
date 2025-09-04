# Search & Evaluation System Requirements

## 1. Introduction

This document specifies the requirements for implementing the **core search and evaluation system** for the Opera chess engine, building on the existing complete board representation and move generation systems. This includes **search (alpha–beta + pruning framework) and handcrafted evaluation**, but **does not include** move generation or neural network integration (those are tracked separately).

**Architecture Overview**: The search and evaluation system leverages the existing complete foundation: Board system (bitboards, FEN, move operations), Move generation (all piece types, legal move validation), Move representation (MoveGen, MoveGenList with 32-bit packed moves), Performance testing infrastructure, UCI FFI bridge (UCIBridge.h with Search interface stubs), and Google Test framework (171/171 tests passing).

## 2. User Stories

### Chess Engine Developers
- **As a chess engine developer**, I want a modular search system with configurable depth and time controls, so that I can optimize the engine's performance for different time controls and hardware capabilities
- **As a chess engine developer**, I want comprehensive transposition table support, so that the engine can avoid recalculating identical positions and achieve competitive search speeds
- **As a chess engine developer**, I want detailed search statistics and debugging information, so that I can analyze the engine's decision-making process and optimize its performance

### Chess Tournament Organizers
- **As a tournament organizer**, I want the engine to respect UCI time limits precisely, so that games finish within allocated time controls without time forfeits
- **As a tournament organizer**, I want the engine to respond to stop commands within 10ms, so that time management is accurate and fair during competitive play
- **As a tournament organizer**, I want configurable playing strength through UCI options, so that the engine can be adapted for different tournament categories

### Chess Players and Analysts
- **As a chess player**, I want the engine to demonstrate Paul Morphy's sacrificial style, so that I can study and learn from romantic-era chess principles
- **As a chess analyst**, I want the engine to find tactical solutions to complex positions, so that I can use it for position analysis and puzzle solving
- **As a chess enthusiast**, I want the engine to show clear variations and evaluation reasoning, so that I can understand why certain moves are preferred

### System Administrators
- **As a system administrator**, I want the engine to use configurable memory for hash tables, so that I can optimize resource usage for different server configurations
- **As a system administrator**, I want deterministic behavior for the same position and search parameters, so that engine testing and debugging is reliable
- **As a system administrator**, I want the engine to integrate with existing UCI infrastructure, so that it works with standard chess GUIs and tournament management systems

## 3. Acceptance Criteria

### Search Algorithm Requirements
- **WHEN** starting search, **THEN** the system **SHALL** implement iterative deepening with aspiration windows
- **WHEN** searching nodes, **THEN** the system **SHALL** use Principal Variation Search (PVS) for optimal tree pruning
- **WHEN** encountering repeated positions, **THEN** the system **SHALL** use transposition table (clustered, replace-by-depth/age) with >90% hit rate
- **WHEN** ordering moves, **THEN** the system **SHALL** prioritize: TT move, captures (MVV-LVA + SEE), killers, counter-moves, history
- **WHEN** reaching leaf nodes, **THEN** the system **SHALL** perform quiescence search (stand-pat, captures, promotions, check evasions)
- **WHEN** optimizing search, **THEN** the system **SHALL** apply pruning & reductions: NMP, LMR, futility, razoring, SEE, delta pruning
- **WHEN** in tactical positions, **THEN** the system **SHALL** apply extensions: check, singular, passed-pawn push
- **WHEN** integrating with existing systems, **THEN** the system **SHALL** use existing `MoveGenList`, `Board::isSquareAttacked()`, UCI async cancellation

### Handcrafted Evaluation Requirements  
- **WHEN** evaluating positions, **THEN** the system **SHALL** calculate material, PST (tapered opening/endgame), mobility
- **WHEN** analyzing pawn structure, **THEN** the system **SHALL** evaluate isolated, doubled, passed pawns, pawn hash + cache
- **WHEN** assessing king safety, **THEN** the system **SHALL** consider attack maps, pawn shield, open files
- **WHEN** measuring development, **THEN** the system **SHALL** track development lead and tempo advantages

### Morphy-Specific Bias Requirements
- **WHEN** MorphyBias option is configured, **THEN** the system **SHALL** apply development bonus: 1.2x weight (opening phase)
- **WHEN** evaluating king attacks, **THEN** the system **SHALL** apply king safety aggression: 1.5x weight for attack evaluation  
- **WHEN** assessing active positions, **THEN** the system **SHALL** apply initiative/tempo bonus: 1.1x weight for active pieces
- **WHEN** considering sacrifices, **THEN** the system **SHALL** provide material sacrifice threshold: 100 centipawn compensation for initiative
- **IF** king is uncastled, **THEN** the system **SHALL** apply uncastled king penalty: 50 centipawn penalty (safety priority)
- **WHEN** configuring style, **THEN** the system **SHALL** support UCI option `MorphyBias` (0.0-2.0 multiplier)

### User Experience Requirements
- **WHEN** starting a search, **THEN** the system **SHALL** begin analysis within 1ms of receiving the go command
- **WHEN** searching, **THEN** the system **SHALL** output progress information every 100ms showing depth, score, nodes searched, and principal variation
- **WHEN** receiving a stop command, **THEN** the system **SHALL** halt search and output best move within 10ms
- **IF** no legal moves are available, **THEN** the system **SHALL** immediately return checkmate or stalemate status without searching

### Performance Requirements
- **WHEN** searching from starting position, **THEN** the system **SHALL** achieve at least 100,000 nodes per second search speed
- **WHEN** using transposition tables, **THEN** the system **SHALL** maintain >90% hit rate for positions occurring in typical middlegame search trees
- **WHEN** ordering moves, **THEN** the system **SHALL** achieve >40% best-move-first rate, placing the eventually best move first in search order
- **WHEN** evaluating positions, **THEN** the system **SHALL** complete each evaluation in under 1 microsecond

### Security Requirements
- **WHEN** receiving search parameters, **THEN** the system **SHALL** validate depth limits (1-64) and time limits (1ms-24 hours) to prevent resource exhaustion
- **WHEN** allocating transposition table memory, **THEN** the system **SHALL** respect system memory limits and fail gracefully if allocation fails
- **IF** search exceeds configured node limits, **THEN** the system **SHALL** terminate search and return the best move found so far

## 4. Technical Architecture

### Core Engine Architecture
- **Framework**: Modern C++17 building on existing Opera Engine foundation
- **Integration**: Uses existing `Board`, `MoveGenList`, `Board::isSquareAttacked()` systems
- **Memory Management**: RAII principles with configurable hash table sizes
- **UCI Coordination**: Compatible with existing `UCIBridge.h` Search interface for async cancellation

### Dependencies (All Complete ✅)
- **Board system** (bitboards, FEN, move operations) - Complete with 100% test coverage
- **Move generation** (all piece types, legal move validation) - Complete implementation  
- **Move representation** (MoveGen, MoveGenList with 32-bit packed moves) - Production ready
- **Performance testing infrastructure** - Comprehensive benchmarking framework
- **UCI FFI bridge** (UCIBridge.h with Search interface stubs) - Ready for integration
- **Google Test framework** - 171/171 tests passing

### Technical Architecture from SEARCH_EVAL.md
```cpp
class SearchEngine {
    Board& board;                         // Use existing board representation
    MoveGenList legal_moves;              // Use existing move generation
    TranspositionTable tt;                
    std::unique_ptr<Evaluator> evaluator; // Strategy pattern for eval
    std::atomic<bool>& stop_flag;         // For UCI async cancellation
};

class MorphyEvaluator : public Evaluator {
private:
    // Configurable Morphy-style weights
    float development_bonus = 1.2f;           // Extra opening development weight
    float king_safety_aggression = 1.5f;      // Aggressive king attack evaluation
    float initiative_tempo_bonus = 1.1f;      // Active piece positioning bonus
    int material_sacrifice_threshold = 100;   // Centipawns compensation for initiative
    int uncastled_king_penalty = 50;         // Safety-first castling incentive
};
```

## 5. Feature Specifications

### Deliverables (From SEARCH_EVAL.md)

#### Search Components:
1. **`search/search_engine.h/.cpp`** — Main search coordinator with UCI integration
2. **`search/alphabeta.h/.cpp`** — Alpha-beta implementation with PVS  
3. **`search/qsearch.h/.cpp`** — Quiescence search
4. **`search/transposition.h/.cpp`** — Transposition table with clustering
5. **`search/move_ordering.h/.cpp`** — Move ordering with history/killers
6. **`search/see.h/.cpp`** — Static exchange evaluation

#### Evaluation Components:
1. **`eval/evaluator_interface.h`** — Abstract evaluator interface
2. **`eval/handcrafted_eval.h/.cpp`** — Standard handcrafted evaluation
3. **`eval/morphy_eval.h/.cpp`** — Morphy-specific evaluation terms

#### Integration & Testing:
1. **Updated existing test files** with search/eval coverage
2. **Performance benchmarks** integrated with existing framework
3. **Launch script** `--search-eval` option for testing

## 6. Success Criteria

### User Experience
- **WHEN** analyzing a tactical position, **THEN** users **SHALL** see the engine find the correct tactical solution within 5 seconds on modern hardware
- **WHEN** playing against the engine, **THEN** users **SHALL** experience Morphy-style sacrificial play that prioritizes initiative over material
- **WHEN** using analysis mode, **THEN** users **SHALL** receive clear principal variations showing the engine's calculated best lines

### Technical Performance (From SEARCH_EVAL.md Performance Targets)
- **WHEN** running search benchmarks, **THEN** the system **SHALL** achieve >100K nodes/second (baseline for competitive play)
- **WHEN** using transposition tables, **THEN** the system **SHALL** achieve >90% hit rate in middlegame positions
- **WHEN** ordering moves, **THEN** the system **SHALL** achieve >40% best-move-first rate
- **WHEN** evaluating positions, **THEN** the system **SHALL** complete evaluation in <1μs per position
- **WHEN** starting search, **THEN** the system **SHALL** achieve <1ms search startup time from "go" command
- **WHEN** stopping search, **THEN** the system **SHALL** respond in <10ms to "stop" command

### Business Goals
- **WHEN** competing in engine tournaments, **THEN** the system **SHALL** demonstrate competitive playing strength at club level (1800+ ELO)
- **WHEN** showcasing Morphy style, **THEN** the system **SHALL** produce aesthetically pleasing games with tactical brilliance
- **WHEN** integrating with UCI systems, **THEN** the system **SHALL** work seamlessly with standard chess GUIs and tournament software

## 7. Assumptions and Dependencies

### Technical Assumptions
- Modern C++17 compiler support with optimization capabilities
- Sufficient memory available for configurable hash table sizes (16MB-2GB range)
- 64-bit architecture for efficient bitboard operations and Zobrist hashing
- Single-threaded search implementation (multi-threading is future enhancement)

### External Dependencies
- Existing Opera Engine board representation and move generation systems
- Google Test framework for comprehensive unit and integration testing
- Rust UCI coordinator through cxx FFI bridge for tournament compatibility
- Standard chess GUI compatibility for user interaction and testing

### Resource Assumptions
- Development team familiar with chess engine algorithms and optimization techniques
- Access to tactical test suites (WAC, ECM, etc.) for validation and benchmarking
- Paul Morphy game collection for style validation and bias term tuning
- Performance testing hardware representing typical user systems

## 8. Constraints and Limitations

### Technical Constraints
- Single-threaded search to maintain FFI compatibility with Rust async system
- Memory usage constrained by configurable hash table size limits
- Evaluation must complete within microsecond timeframes for competitive performance
- FFI interface must remain compatible with existing UCIBridge.h definitions

### Business Constraints
- Implementation timeline constrained by existing UCI protocol development schedule
- Performance targets must be achievable on consumer hardware (not specialized chess computers)
- Code quality standards require >95% test coverage and comprehensive documentation
- Morphy style implementation balanced with competitive playing strength requirements

### Regulatory Constraints
- UCI protocol compliance for tournament and GUI compatibility
- Fair play requirements - no opening books or endgame tablebase integration in core search
- Deterministic behavior for testing and debugging reproducibility
- Memory safety and resource management for stable long-running operation

## 9. Risk Assessment

### Technical Risks
- **Risk**: Complex search algorithms may introduce bugs affecting playing strength
  - **Likelihood**: Medium
  - **Impact**: High
  - **Mitigation**: Comprehensive test suite with tactical positions and perft validation

- **Risk**: FFI integration complexity may cause stability issues in tournament play
  - **Likelihood**: Medium
  - **Impact**: High  
  - **Mitigation**: Extensive integration testing with Rust UCI coordinator and error boundaries

- **Risk**: Performance optimization may conflict with code maintainability
  - **Likelihood**: Medium
  - **Impact**: Medium
  - **Mitigation**: Profile-guided optimization with clear performance benchmarks and code documentation

### Business Risks
- **Risk**: Engine may not achieve competitive playing strength against established engines
  - **Likelihood**: Medium
  - **Impact**: Medium
  - **Mitigation**: Focus on Morphy style differentiation rather than pure ELO rating competition

- **Risk**: Implementation timeline may exceed UCI protocol development schedule
  - **Likelihood**: Low
  - **Impact**: Medium
  - **Mitigation**: Phased implementation approach with search before advanced evaluation features

### User Experience Risks
- **Risk**: Morphy style biases may result in poor play in certain position types
  - **Likelihood**: Medium
  - **Impact**: Medium
  - **Mitigation**: Configurable bias strength with traditional evaluation fallback option

## 10. Non-Functional Requirements

### Scalability
- Hash table size configurable from 16MB to 2GB based on available system memory
- Search depth scalable from 1 to 64 plies based on time control and position complexity
- Node count limits configurable for analysis mode and time-controlled play

### Availability
- 24/7 operation capability for tournament and server deployment
- Graceful degradation when memory allocation fails or system resources are constrained
- Automatic recovery from transposition table corruption or hash collisions

### Maintainability
- Modular design with clear interfaces between search, evaluation, and integration components
- Comprehensive unit test coverage >95% with integration tests for all major features
- Clear documentation of algorithms, optimization techniques, and Morphy-style implementations
- Code follows Opera Engine standards with consistent formatting and commenting

### Usability
- UCI option interface for all configurable parameters (hash size, Morphy bias, search depth)
- Clear error messages for invalid search parameters or system resource limitations
- Detailed progress output during search including depth, score, nodes, and principal variation
- Deterministic operation for the same input parameters to enable testing and debugging

## 11. Out of Scope (From SEARCH_EVAL.md)
- **Move generation** (✅ already complete)
- **Neural net eval** (future phase)  
- **SMP search** (future enhancement)
- **Syzygy/EGTB probing** (future enhancement)

## 12. Final Acceptance Criteria (From SEARCH_EVAL.md)
- [ ] **Test Coverage**: Near 100% coverage for all search/eval components
- [ ] **Correctness**: Perft validation passes (d1–d6) with search integrated
- [ ] **Tactical Strength**: Tactical EPD threshold met (>70% tactical puzzles solved)
- [ ] **Morphy Style**: Style EPD confirms sacrificial bias (custom test suite)
- [ ] **UCI Integration**: All UCI options exposed (`MorphyBias`, `Hash`, `LMRStrength`, etc.)
- [ ] **Performance**: All performance targets achieved in benchmarking
- [ ] **FFI Compatibility**: Works correctly with Rust UCI coordinator
- [ ] **Deterministic**: Builds and runs produce consistent results
- [ ] **Launch Integration**: `./launch.sh --search-eval` option functional

---

**Document Status**: Draft

**Last Updated**: 2025-01-02

**Stakeholders**: Opera Engine development team, chess engine experts, UCI protocol specialists

**Related Documents**: SEARCH_EVAL.md, UCI Protocol Design Document, CLAUDE.md project guidelines

**Version**: 1.0