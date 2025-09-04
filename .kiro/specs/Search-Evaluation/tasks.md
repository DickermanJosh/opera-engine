# Search & Evaluation System Implementation Tasks

## Task Overview

This document breaks down the implementation of the Search & Evaluation system for Opera Chess Engine into actionable coding tasks. Each task is designed to build incrementally upon the existing foundation (Board, MoveGen, UCI bridge) while maintaining 100% test coverage and competitive performance.

**Total Estimated Tasks**: 18 tasks organized into 4 phases

**Requirements Reference**: This implementation addresses all requirements from `requirements.md` with focus on alpha-beta search, handcrafted evaluation, and Morphy-style playing characteristics.

**Design Reference**: Technical approach defined in `design.md` with SearchEngine coordination, PVS optimization, and modular evaluator architecture.

## Implementation Tasks

### Phase 1: Core Search Infrastructure (4 tasks)

#### **1.1** ✅ Create Search Engine Foundation and Interface **COMPLETED**
- **Status**: ✅ **COMPLETED** - 2025-01-02
- **Description**: Implement the main `SearchEngine` class with UCI integration, search limits management, and coordination between search algorithms and evaluation
- **Requirements Addressed**: R1-R4 (Search algorithm requirements), R19-R22 (User experience requirements)
- **Deliverables**: ✅ **ALL COMPLETED**
  - ✅ `cpp/include/search/search_engine.h` - SearchEngine class definition with UCI integration
  - ✅ `cpp/src/search/search_engine.cpp` - SearchEngine implementation with iterative deepening framework
  - ✅ `cpp/tests/SearchEngineTest.cpp` - Comprehensive unit tests for search coordination (19 tests)
  - ✅ Integration with existing `UCIBridge.h` interface for FFI compatibility
- **Actual Effort**: 4 hours (TDD approach was highly efficient)
- **Dependencies**: ✅ Existing Board system, MoveGen, UCIBridge.h
- **Acceptance Criteria**: ✅ **ALL MET**
  - ✅ SearchEngine manages search limits (depth, time, nodes) correctly
  - ✅ UCI integration works with existing FFI bridge  
  - ✅ Atomic stop flag coordination for async cancellation
  - ✅ Search info updates every 100ms during search
  - ✅ All unit tests pass with 100% success rate (19/19 tests)
  - ✅ Performance: <1ms search startup time achieved
  - ✅ No regressions: All 171 existing tests continue to pass

#### **1.2** Implement Transposition Table with Clustering ✅ **COMPLETED**
- **Description**: Create clustered transposition table using Zobrist keys with replace-by-depth/age strategy for >90% hit rate in middlegame positions
- **Requirements Addressed**: R3 (Transposition table requirement), R23-R28 (Performance requirements)
- **Deliverables**:
  - ✅ `cpp/include/search/transposition_table.h` - TTEntry structure and TranspositionTable class
  - ✅ `cpp/src/search/transposition_table.cpp` - Clustered hash table implementation
  - ✅ `cpp/tests/TranspositionTableTest.cpp` - TT functionality and performance tests
  - ✅ Platform-portable prefetching support and memory management
- **Actual Effort**: 4 hours (matched estimate)
- **Dependencies**: 1.1, existing Board Zobrist keys
- **Acceptance Criteria**:
  - ✅ Clustered design with 4 entries per cluster (TTCluster::CLUSTER_SIZE = 4)
  - ✅ Replace-by-depth/age strategy implementation with intelligent priority scoring
  - ✅ Packed 128-bit TTEntry structure for cache efficiency (≤16 bytes per entry)
  - ✅ Configurable memory size (1MB-128MB+) with proper size calculations
  - ✅ Thread-safe probe/store operations with atomic statistics tracking
  - ✅ **Performance Achieved**: <100μs per operation, hit rate tracking available
  - ✅ **Testing**: 20/20 comprehensive tests passing covering all functionality
  - ✅ **Memory Optimization**: Clustering provides optimal cache utilization
  - ✅ **Platform Compatibility**: Works on ARM/x86 with portable prefetch macros

#### **1.3** Create Move Ordering System with Multi-Stage Scoring
- **Description**: Implement comprehensive move ordering with TT moves, MVV-LVA captures, killer moves, and history heuristics targeting >40% best-move-first rate
- **Requirements Addressed**: R4 (Move ordering requirement), R23-R28 (Performance requirements)
- **Deliverables**:
  - `cpp/include/search/move_ordering.h` - MoveOrdering class and scoring constants
  - `cpp/src/search/move_ordering.cpp` - Multi-stage move scoring implementation
  - `cpp/tests/MoveOrderingTest.cpp` - Move ordering effectiveness tests
  - Integration with existing `MoveGenList` for efficient sorting
- **Estimated Effort**: 5 hours
- **Dependencies**: 1.1, 1.2, existing MoveGen system
- **Acceptance Criteria**:
  - TT move prioritization (10000 points)
  - Good captures with MVV-LVA scoring (8000+ points)
  - Killer move integration (6000 points)
  - History heuristic implementation (1000+ points)
  - >40% best-move-first rate in tactical positions

#### **1.4** Implement Static Exchange Evaluation (SEE)
- **Description**: Create SEE for accurate capture evaluation, essential for move ordering and quiescence search quality
- **Requirements Addressed**: R4 (Move ordering with SEE), R6 (Quiescence captures)
- **Deliverables**:
  - `cpp/include/search/see.h` - StaticExchangeEvaluator class definition
  - `cpp/src/search/see.cpp` - Capture sequence evaluation implementation
  - `cpp/tests/StaticExchangeTest.cpp` - SEE correctness tests with tactical positions
  - Integration with existing `Board::isSquareAttacked()` functionality
- **Estimated Effort**: 4 hours
- **Dependencies**: 1.3, existing Board attack detection
- **Acceptance Criteria**:
  - Accurate capture sequence evaluation
  - Integration with move ordering for good/bad capture classification
  - Performance <10μs per SEE evaluation
  - Correct handling of X-ray attacks and pinned pieces

### Phase 2: Alpha-Beta Search Implementation (4 tasks)

#### **2.1** Create Core Alpha-Beta Search with PVS
- **Description**: Implement the main alpha-beta search algorithm with Principal Variation Search optimization for efficient tree pruning
- **Requirements Addressed**: R1-R2 (Iterative deepening, PVS), R7 (Extensions)
- **Deliverables**:
  - `cpp/include/search/alphabeta.h` - AlphaBetaSearch class with PVS interface
  - `cpp/src/search/alphabeta.cpp` - Core search implementation with PVS optimization
  - `cpp/tests/AlphaBetaTest.cpp` - Search algorithm correctness and performance tests
  - Integration with SearchEngine for iterative deepening coordination
- **Estimated Effort**: 8 hours
- **Dependencies**: 1.1, 1.2, 1.3, 1.4
- **Acceptance Criteria**:
  - PVS implementation with null-window searches for non-PV nodes
  - Check extension, singular extension, passed-pawn extension
  - Killer move and history table updates
  - Search statistics tracking (nodes, depth, time)
  - >100K nodes/second search performance

#### **2.2** Implement Quiescence Search
- **Description**: Create quiescence search to resolve tactical sequences and avoid horizon effects
- **Requirements Addressed**: R5 (Quiescence search), R6 (Stand-pat, captures, check evasions)
- **Deliverables**:
  - `cpp/include/search/qsearch.h` - QuiescenceSearch class definition
  - `cpp/src/search/qsearch.cpp` - Tactical sequence resolution implementation
  - `cpp/tests/QuiescenceTest.cpp` - Quiescence search correctness tests
  - Integration with SEE for capture quality assessment
- **Estimated Effort**: 4 hours
- **Dependencies**: 2.1, 1.4 (SEE)
- **Acceptance Criteria**:
  - Stand-pat evaluation as position lower bound
  - Capture-only search (captures, promotions, check evasions)
  - Delta pruning for futile captures
  - SEE integration for capture evaluation
  - Proper handling of checkmate/stalemate in QSearch

#### **2.3** Add Search Optimizations (Pruning and Reductions)
- **Description**: Implement advanced search optimizations including null move pruning, late move reductions, and futility pruning
- **Requirements Addressed**: R7 (Pruning & reductions: NMP, LMR, futility, razoring)
- **Deliverables**:
  - Extensions to `alphabeta.cpp` with pruning and reduction implementations
  - `cpp/tests/SearchOptimizationTest.cpp` - Pruning effectiveness and correctness tests
  - UCI options for tuning search parameters (LMRStrength, NullMoveReduction)
  - Performance validation showing effective branching factor reduction
- **Estimated Effort**: 6 hours
- **Dependencies**: 2.1, 2.2
- **Acceptance Criteria**:
  - Null move pruning with R=3 reduction
  - Late move reductions (1-3 plies) for non-PV nodes
  - Futility pruning in leaf nodes
  - Razoring in pre-frontier nodes
  - Effective branching factor <4 with optimizations

#### **2.4** Integrate Aspiration Windows and Search Control
- **Description**: Add aspiration windows for iterative deepening and comprehensive search control with time management
- **Requirements Addressed**: R1 (Aspiration windows), R19-R22 (UCI integration, timing)
- **Deliverables**:
  - Aspiration window implementation in `search_engine.cpp`
  - Time management integration with search limits
  - Progress reporting and search info output
  - Emergency stop handling for hard time limits
- **Estimated Effort**: 3 hours
- **Dependencies**: 2.1, 2.2, 2.3
- **Acceptance Criteria**:
  - Aspiration windows ±25cp initially, widening on fail-high/low
  - <1ms search startup time from go command
  - <10ms stop response time
  - Complete depth guarantee before time expiration
  - Search info output every 100ms with depth, score, PV

### Phase 3: Evaluation System Implementation (6 tasks)

#### **3.1** Create Abstract Evaluator Interface
- **Description**: Implement the strategy pattern evaluator interface enabling future neural network integration while supporting current handcrafted approach
- **Requirements Addressed**: R33 (Modular evaluator interface), Future extensibility
- **Deliverables**:
  - `cpp/include/eval/evaluator_interface.h` - Abstract Evaluator base class
  - `cpp/tests/EvaluatorInterfaceTest.cpp` - Interface contract tests
  - Plugin architecture for different evaluation approaches
  - Incremental evaluation hooks for optimization
- **Estimated Effort**: 2 hours
- **Dependencies**: None (foundational)
- **Acceptance Criteria**:
  - Pure virtual interface with evaluate() method
  - Configuration method for UCI options
  - Optional incremental update hooks
  - Clear documentation for future implementers

#### **3.2** Implement Handcrafted Evaluator Foundation
- **Description**: Create traditional chess evaluation covering material, piece-square tables, and basic positional factors
- **Requirements Addressed**: R9-R12 (Handcrafted evaluation requirements)
- **Deliverables**:
  - `cpp/include/eval/handcrafted_eval.h` - HandcraftedEvaluator class
  - `cpp/src/eval/handcrafted_eval.cpp` - Material and PST implementation
  - `cpp/tests/HandcraftedEvalTest.cpp` - Evaluation correctness tests
  - Tapered evaluation for opening/middlegame/endgame phases
- **Estimated Effort**: 5 hours
- **Dependencies**: 3.1
- **Acceptance Criteria**:
  - Standard piece values (P=100, N=320, B=330, R=500, Q=900)
  - Piece-square tables for all pieces
  - Opening/endgame phase detection and interpolation
  - <1μs per evaluation performance requirement
  - Evaluation from white perspective with side-to-move adjustment

#### **3.3** Add Advanced Positional Evaluation
- **Description**: Implement sophisticated positional factors including king safety, pawn structure, and piece mobility
- **Requirements Addressed**: R10-R11 (Pawn structure, king safety), R12 (Development, tempo)
- **Deliverables**:
  - Extensions to `handcrafted_eval.cpp` with positional evaluation
  - Pawn structure analysis (isolated, doubled, passed pawns)
  - King safety evaluation with pawn shield and attack analysis
  - Piece mobility and development assessment
- **Estimated Effort**: 6 hours
- **Dependencies**: 3.2, existing Board attack detection
- **Acceptance Criteria**:
  - Pawn structure penalties: isolated (-20cp), doubled (-10cp)
  - Passed pawn bonuses scaling with advancement
  - King safety based on pawn shield and open files
  - Development bonuses in opening phase
  - Mobility calculation for all pieces

#### **3.4** Create Morphy Evaluator Specialization
- **Description**: Implement Paul Morphy's sacrificial style with specific bias terms for development, king attacks, and initiative
- **Requirements Addressed**: R13-R18 (Morphy-specific bias requirements)
- **Deliverables**:
  - `cpp/include/eval/morphy_eval.h` - MorphyEvaluator class definition
  - `cpp/src/eval/morphy_eval.cpp` - Morphy-style evaluation implementation
  - `cpp/tests/MorphyEvalTest.cpp` - Style-specific evaluation tests
  - UCI option integration for MorphyBias configuration (0.0-2.0)
- **Estimated Effort**: 5 hours
- **Dependencies**: 3.2, 3.3
- **Acceptance Criteria**:
  - Development bonus: 1.2x weight in opening phase
  - King safety aggression: 1.5x weight for attack evaluation
  - Initiative/tempo bonus: 1.1x weight for active pieces
  - Material sacrifice threshold: 100cp compensation
  - Uncastled king penalty: 50cp (safety priority)
  - Configurable bias multiplier via UCI

#### **3.5** Add Sacrifice Recognition and Compensation
- **Description**: Implement sophisticated sacrifice detection and compensation logic for Morphy's tactical style
- **Requirements Addressed**: R15 (Material sacrifice threshold), R16 (Initiative compensation)
- **Deliverables**:
  - Sacrifice detection algorithms in `morphy_eval.cpp`
  - Initiative and tempo measurement functions
  - Compensation calculation for material deficits
  - Tactical pattern recognition for sacrificial motifs
- **Estimated Effort**: 4 hours
- **Dependencies**: 3.4, existing move generation for tactical analysis
- **Acceptance Criteria**:
  - Detection of material sacrifices vs positional compensation
  - Initiative measurement through piece activity and tempo
  - Up to 100cp bonus for sacrificial positions with initiative
  - Pattern recognition for common sacrificial motifs
  - Integration with search for tactical sequence evaluation

#### **3.6** Implement Evaluation Caching and Optimization
- **Description**: Add evaluation caching and optimization techniques for competitive performance requirements
- **Requirements Addressed**: R23-R28 (Performance requirements), Pawn hash caching
- **Deliverables**:
  - Pawn structure hash table implementation
  - Evaluation result caching for repeated positions
  - Incremental evaluation updates for make/unmake moves
  - Performance optimization and profiling
- **Estimated Effort**: 3 hours
- **Dependencies**: 3.2, 3.3, 3.4
- **Acceptance Criteria**:
  - Pawn hash table with >95% hit rate
  - <1μs average evaluation time
  - Incremental updates for 80%+ of evaluations
  - Memory usage <10MB for evaluation caches
  - Performance benchmarking integration

### Phase 4: Integration, Testing, and Optimization (4 tasks)

#### **4.1** **[CRITICAL]** Complete Search-Evaluation Integration
- **Description**: Integrate all search and evaluation components into a cohesive system with comprehensive testing
- **Requirements Addressed**: All requirements R1-R32, comprehensive system integration
- **Deliverables**:
  - Complete integration in `search_engine.cpp`
  - Cross-component integration tests
  - Full system performance validation
  - UCI option registration for all configurable parameters
- **Estimated Effort**: 4 hours
- **Dependencies**: All previous tasks (1.1-3.6)
- **Acceptance Criteria**:
  - All search and evaluation components work together
  - UCI options exposed: Hash, MorphyBias, LMRStrength, etc.
  - Search performance >100K nodes/second
  - Evaluation performance <1μs per position
  - Integration tests pass for all major features

#### **4.2** Create Comprehensive Test Suite
- **Description**: Implement comprehensive unit, integration, and performance tests achieving >95% code coverage
- **Requirements Addressed**: R35 (Near 100% coverage), R36-R39 (Acceptance criteria)
- **Deliverables**:
  - Complete unit test coverage for all components
  - Integration tests for search-evaluation coordination
  - Performance regression tests and benchmarks
  - Tactical EPD test suite for correctness validation
- **Estimated Effort**: 6 hours
- **Dependencies**: 4.1
- **Acceptance Criteria**:
  - >95% code coverage across all search/eval components
  - Perft validation passes (d1-d6) with search integrated
  - >70% tactical puzzle solution rate
  - Performance benchmarks meet all targets
  - Memory usage validation and leak detection

#### **4.3** Implement Style Validation and Tactical Testing
- **Description**: Create specialized test suites for Morphy style validation and tactical strength assessment
- **Requirements Addressed**: R37 (Tactical EPD threshold), R38 (Morphy style confirmation)
- **Deliverables**:
  - Tactical test suite integration (WAC, ECM, etc.)
  - Morphy style EPD test positions
  - Sacrificial motif recognition tests
  - Style bias validation and tuning
- **Estimated Effort**: 4 hours
- **Dependencies**: 4.1, 4.2
- **Acceptance Criteria**:
  - >70% tactical puzzle solution rate within time limits
  - Custom Morphy style test suite validates sacrificial bias
  - Style EPD confirms development priority and king attack focus
  - Bias configuration validation through test positions
  - Performance comparison with/without Morphy bias

#### **4.4** **[CRITICAL]** Production Integration and Launch
- **Description**: Complete integration with existing build system, launch script, and UCI infrastructure for production readiness
- **Requirements Addressed**: R39-R40 (FFI compatibility, launch integration), Production deployment
- **Deliverables**:
  - CMakeLists.txt integration for search/eval components
  - Launch script `--search-eval` and `--search-bench` options
  - FFI interface completion in `UCIBridge.cpp`
  - Documentation updates and performance reports
- **Estimated Effort**: 3 hours
- **Dependencies**: 4.1, 4.2, 4.3
- **Acceptance Criteria**:
  - `./launch.sh --search-eval` runs complete test suite
  - `./launch.sh --search-bench` executes performance benchmarks
  - FFI integration works correctly with Rust UCI coordinator
  - Build system compiles without errors or warnings
  - All 171+ existing tests continue to pass

## Task Guidelines

### Task Completion Criteria
Each task is considered complete when:
- [ ] All deliverables are implemented and functional
- [ ] Unit tests achieve >95% coverage with comprehensive edge cases
- [ ] Integration tests validate cross-component functionality
- [ ] Performance requirements are met and validated
- [ ] Code follows Opera Engine standards (C++17, RAII, const-correctness)
- [ ] Documentation includes clear API descriptions and usage examples
- [ ] All linked requirements are satisfied and traceable
- [ ] FFI compatibility maintained with existing UCIBridge.h interface

### Testing Requirements
- **Unit Tests**: Required for all public methods with edge cases and error conditions
- **Integration Tests**: Required for search-evaluation coordination and UCI compatibility
- **Performance Tests**: Required with specific benchmarks (NPS, TT hit rate, move ordering)
- **Tactical Tests**: Required with EPD test suites for correctness validation
- **Style Tests**: Required with Morphy-specific positions for bias validation
- **Memory Tests**: Required with leak detection and usage profiling
- **Regression Tests**: Required to ensure existing functionality remains intact

### Code Quality Standards
- Modern C++17 with const-correctness and RAII patterns
- Template optimization for performance-critical code paths
- Clear separation of concerns between search, evaluation, and coordination
- Comprehensive error handling with graceful degradation
- Performance-first design with <1μs evaluation, >100K NPS search
- Integration with existing systems (Board, MoveGen, UCI bridge)
- Consistent naming and documentation following project conventions

## Progress Tracking

### Milestone Checkpoints
- **Milestone 1**: Search Infrastructure Complete - [Phase 1: 1/4 tasks complete] - Target: Day 3
  - ✅ Task 1.1: Search Engine Foundation (COMPLETED 2025-01-02)
  - ⏳ Task 1.2: Transposition Table (IN PROGRESS)
  - ⏳ Task 1.3: Move Ordering System
  - ⏳ Task 1.4: Static Exchange Evaluation
- **Milestone 2**: Alpha-Beta Implementation Ready - [Phase 2 Complete] - Target: Day 7
- **Milestone 3**: Evaluation System Functional - [Phase 3 Complete] - Target: Day 12
- **Milestone 4**: Production Integration Complete - [Phase 4 Complete] - Target: Day 15

### Definition of Done
A task is considered "Done" when:
1. **Functionality**: All specified functionality implemented and tested
2. **Performance**: Meets all performance requirements under benchmark testing
3. **Integration**: Works correctly with existing Opera Engine systems
4. **Testing**: Comprehensive test coverage with multiple validation strategies
5. **Documentation**: Clear code documentation and API descriptions
6. **Requirements**: All linked requirements satisfied and verified
7. **Quality**: Passes all code quality checks and performance standards
8. **Compatibility**: Maintains FFI compatibility with Rust UCI coordination
9. **Style**: Morphy-specific features validated against style requirements
10. **Production**: Ready for integration with tournament and GUI systems

## Risk Mitigation

### Technical Risks
- **Risk**: Search algorithm complexity causing performance bottlenecks
  - **Mitigation**: Incremental optimization with performance benchmarking after each task
  - **Affected Tasks**: 2.1, 2.2, 2.3

- **Risk**: Evaluation complexity impacting search speed  
  - **Mitigation**: <1μs evaluation requirement with caching and incremental updates
  - **Affected Tasks**: 3.2, 3.3, 3.6

- **Risk**: FFI integration complexity with Rust coordination
  - **Mitigation**: Maintain existing UCIBridge.h interface, comprehensive integration testing
  - **Affected Tasks**: 1.1, 4.1, 4.4

### Implementation Risks
- **Risk**: Morphy style bias conflicting with playing strength
  - **Mitigation**: Configurable bias multiplier with traditional evaluation fallback
  - **Affected Tasks**: 3.4, 3.5

- **Risk**: Memory usage exceeding system limits
  - **Mitigation**: Configurable hash sizes with validation and graceful degradation
  - **Affected Tasks**: 1.2, 3.6

### Timeline Risks
- **Risk**: Performance optimization taking longer than estimated
  - **Mitigation**: Parallel development with performance testing throughout
  - **Affected Tasks**: All tasks with performance requirements

## Resource Requirements

### Development Environment
- Modern C++17 compiler with optimization support (-O3, -march=native)
- Existing Opera Engine build system (CMake, Google Test)
- Performance profiling tools (perf, Intel VTune, or similar)
- Memory debugging tools (Valgrind, AddressSanitizer)
- Chess position databases and tactical test suites (EPD format)

### External Dependencies
- Existing Opera Engine foundation (Board, MoveGen, UCI bridge)
- Google Test framework for comprehensive testing
- Tactical test suites (WAC, ECM, Bratko-Kopec) for validation
- Paul Morphy game collection for style validation
- Performance benchmarking positions and reference engines

### Performance Validation
- Target hardware: Modern multi-core CPU (Intel/AMD x64)
- Memory requirements: 16MB-2GB configurable hash table
- Performance baselines: >100K NPS, >90% TT hit rate, >40% move ordering
- Comparison engines for strength and style validation
- Tournament time controls for realistic testing scenarios

---

**Task Status**: Phase 1 In Progress (1/4 tasks complete)

**Current Phase**: Phase 1 - Core Search Infrastructure

**Current Task**: 1.2 - Transposition Table with Clustering

**Overall Progress**: 5.6% complete (1/18 tasks)

**Phase 1 Progress**: 25% complete (1/4 tasks)

**Overall Complexity**: High (Advanced algorithms with performance requirements)

**Estimated Completion**: 15 development days (on track)

**Dependencies**: Complete Opera Engine foundation (✅ all ready)

**Success Metrics**: >100K NPS, >70% tactical solutions, Morphy style validation

**Last Updated**: 2025-01-02

**Recent Achievement**: ✅ Task 1.1 completed with full TDD approach (19/19 tests passing)

**Assigned Developer**: Implementation team with chess algorithm expertise

**Technical Reviewers**: Search algorithm experts, performance optimization specialists, chess engine architects