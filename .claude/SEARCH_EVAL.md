# Core Search & Eval (Engine)

### Overview
Implement the **core search and evaluation system** for the Opera chess engine.  
This includes **search (alpha–beta + pruning framework) and handcrafted evaluation**, building on the existing complete board representation and move generation systems.

### Dependencies
- ✅ **Board system** (bitboards, FEN, move operations) - Complete with 100% test coverage
- ✅ **Move generation** (all piece types, legal move validation) - Complete implementation
- ✅ **Move representation** (MoveGen, MoveGenList with 32-bit packed moves) - Production ready
- ✅ **Performance testing infrastructure** - Comprehensive benchmarking framework
- ✅ **UCI FFI bridge** (UCIBridge.h with Search interface stubs) - Ready for integration
- ✅ **Google Test framework** - 171/171 tests passing

---

### Scope

#### In Scope
- **Search**
  - Iterative deepening with aspiration windows
  - Principal Variation Search (PVS)
  - Transposition table (clustered, replace-by-depth/age)
  - Move ordering: TT move, captures (MVV-LVA + SEE), killers, counter-moves, history
  - Quiescence search (stand-pat, captures, promotions, check evasions)
  - Pruning & reductions: NMP, LMR, futility, razoring, SEE, delta pruning
  - Extensions: check, singular, passed-pawn push
  - **Integration**: Uses existing `MoveGenList`, `Board::isSquareAttacked()`, UCI async cancellation

- **Evaluation (handcrafted only)**
  - Material, PST (tapered opening/endgame), mobility
  - Pawn structure (isolated, doubled, passed pawns, pawn hash + cache)
  - King safety (attack maps, pawn shield, open files)
  - Development lead, tempo
  - **Morphy-Specific Bias Terms** (configurable weights):
    - Development bonus: 1.2x weight (opening phase)
    - King safety aggression: 1.5x weight for attack evaluation
    - Initiative/tempo bonus: 1.1x weight for active pieces
    - Material sacrifice threshold: 100 centipawn compensation for initiative
    - Uncastled king penalty: 50 centipawn penalty (safety priority)
  - Configurable via UCI option `MorphyBias` (0.0-2.0 multiplier)

- **Interfaces**
  - Modular `Evaluator` interface with incremental hooks
  - TT probe/store API  
  - UCI options for tuning search/eval
  - **FFI Integration**: Compatible with existing `UCIBridge.h` Search interface for Rust coordination

- **Testing**
  - Extend existing GoogleTest framework (171 tests currently passing)
  - Integrate with existing PerformanceTest infrastructure
  - Maintain near 100% coverage
  - Perft, tactical EPD, style EPD for sacrificial motifs

#### Out of Scope
- Move generation (✅ already complete)
- Neural net eval (future phase)
- SMP search (future enhancement)
- Syzygy/EGTB probing (future enhancement)

---

### Technical Architecture

#### Core Search Engine Integration
```cpp
class SearchEngine {
    Board& board;                         // Use existing board representation
    MoveGenList legal_moves;              // Use existing move generation
    TranspositionTable tt;                
    std::unique_ptr<Evaluator> evaluator; // Strategy pattern for eval
    std::atomic<bool>& stop_flag;         // For UCI async cancellation
    
public:
    SearchEngine(Board& b, std::atomic<bool>& stop);
    SearchResult search(const SearchLimits& limits);
    void stop();
};
```

#### Morphy Evaluator Specification
```cpp
class MorphyEvaluator : public Evaluator {
private:
    // Configurable Morphy-style weights
    float development_bonus = 1.2f;           // Extra opening development weight
    float king_safety_aggression = 1.5f;      // Aggressive king attack evaluation
    float initiative_tempo_bonus = 1.1f;      // Active piece positioning bonus
    int material_sacrifice_threshold = 100;   // Centipawns compensation for initiative
    int uncastled_king_penalty = 50;         // Safety-first castling incentive
    
public:
    int evaluate(const Board& board, Color side_to_move) override;
    void configure_morphy_bias(float bias_multiplier); // UCI option integration
};
```

---

### Deliverables
**Search Components:**
- `search/search_engine.h/.cpp` — Main search coordinator with UCI integration
- `search/alphabeta.h/.cpp` — Alpha-beta implementation with PVS
- `search/qsearch.h/.cpp` — Quiescence search
- `search/transposition.h/.cpp` — Transposition table with clustering
- `search/move_ordering.h/.cpp` — Move ordering with history/killers
- `search/see.h/.cpp` — Static exchange evaluation

**Evaluation Components:**
- `eval/evaluator_interface.h` — Abstract evaluator interface
- `eval/handcrafted_eval.h/.cpp` — Standard handcrafted evaluation
- `eval/morphy_eval.h/.cpp` — Morphy-specific evaluation terms

**Integration & Testing:**
- Updated existing test files with search/eval coverage
- Performance benchmarks integrated with existing framework
- Launch script `--search-eval` option for testing

---

### Performance Targets
- **Search speed**: >100K nodes/second (baseline for competitive play)
- **Transposition table efficiency**: >90% hit rate in middlegame positions  
- **Move ordering effectiveness**: First move best >40% of the time
- **Evaluation speed**: <1μs per position evaluation
- **UCI integration**: <1ms search startup time from "go" command
- **Search stopping**: <10ms response time to "stop" command

### Acceptance Criteria
- [ ] **Test Coverage**: Near 100% coverage for all search/eval components
- [ ] **Correctness**: Perft validation passes (d1–d6) with search integrated
- [ ] **Tactical Strength**: Tactical EPD threshold met (>70% tactical puzzles solved)
- [ ] **Morphy Style**: Style EPD confirms sacrificial bias (custom test suite)
- [ ] **UCI Integration**: All UCI options exposed (`MorphyBias`, `Hash`, `LMRStrength`, etc.)
- [ ] **Performance**: All performance targets achieved in benchmarking
- [ ] **FFI Compatibility**: Works correctly with Rust UCI coordinator
- [ ] **Deterministic**: Builds and runs produce consistent results
- [ ] **Launch Integration**: `./launch.sh --search-eval` option functional