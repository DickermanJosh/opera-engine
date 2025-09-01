# Opera Engine — UCI Strategy Plan (Rust)

Instruction to Claude: Use this document as the authoritative plan when generating Rust UCI code for Opera Engine.

> Purpose: Define how Rust will own the **UCI-facing executable** for Opera Engine, bridging to the C++ core (search/movegen) and remaining independent of the Python NN training stack. This document is **implementation-agnostic** (no code), focused on roles, boundaries, and best practices.

---

## 1. Goals & Non-Goals
**Goals**
- Strict UCI compliance (all required commands).
- Robust error handling, never panic on malformed input.
- Low latency control path (instant `stop`/`ponderhit`).
- Clean FFI boundary to C++ core, with cancellation + time controls.
- Deterministic logging/telemetry for reproducibility.
- Testable with transcripts, fuzzing, and CI harnesses.

**Non-Goals**
- Implementing chess search/eval in Rust.
- Running Python NN code inside the UCI binary.

---

## 2. Architecture
UCI Protocol Implementation Plan - Rust Coordination Layer
Overview
Implement a comprehensive UCI (Universal Chess Interface) protocol handler in Rust that coordinates between the C++ engine core and Python AI wrapper, serving as the intelligent orchestration layer for the Opera chess engine.
Architecture Philosophy
Core Principles

Coordination First: Rust serves as the intelligent bridge between C++ speed and Python intelligence
Async by Design: Handle concurrent UCI commands (search/stop) naturally with Tokio
Type Safety: Leverage Rust's type system for robust UCI command parsing and validation
Zero-Copy Performance: Efficient string handling and memory management
Extensible Integration: Easy platform connectors for Lichess, custom apps, analysis tools

Multi-Language Stack Integration
┌─────────────────────────────────────────┐
│              GUI/Platform               │
├─────────────────────────────────────────┤
│         Rust UCI Coordinator            │  ← NEW LAYER
│  - Protocol parsing & validation        │
│  - Async command handling               │
│  - C++/Python coordination             │
│  - Configuration management             │
├─────────────────────────────────────────┤
│     C++ Engine Core     │ Python AI     │
│   - Move generation     │ - Morphy AI   │
│   - Search algorithms   │ - TensorFlow  │  
│   - Board evaluation    │ - Heuristics  │
└─────────────────────────────────────────┘
Project Structure
Updated Monorepo Layout
opera-engine/
├── rust/                    # Rust UCI coordination layer
│   ├── src/
│   │   ├── main.rs         # Entry point and CLI handling
│   │   ├── uci/            # UCI protocol implementation
│   │   │   ├── mod.rs      # Module exports
│   │   │   ├── parser.rs   # Command parsing
│   │   │   ├── handler.rs  # Command handlers
│   │   │   ├── engine.rs   # Engine interface
│   │   │   ├── options.rs  # UCI options management
│   │   │   └── types.rs    # UCI data types
│   │   ├── bridge/         # FFI bindings
│   │   │   ├── mod.rs
│   │   │   ├── cpp.rs      # C++ engine bindings
│   │   │   └── python.rs   # Python AI bindings
│   │   ├── search/         # Search coordination
│   │   │   ├── mod.rs
│   │   │   ├── coordinator.rs  # C++/Python coordination
│   │   │   ├── morphy.rs   # Morphy-style integration
│   │   │   └── async_search.rs # Async search handling
│   │   ├── config/         # Configuration system
│   │   │   ├── mod.rs
│   │   │   ├── engine.rs   # Engine configuration
│   │   │   └── platforms.rs # Platform-specific configs
│   │   ├── platforms/      # Platform integrations
│   │   │   ├── mod.rs
│   │   │   ├── lichess.rs  # Lichess bot API
│   │   │   ├── analysis.rs # Analysis mode
│   │   │   └── custom.rs   # Custom app connectors
│   │   └── utils/          # Utilities
│   │       ├── mod.rs
│   │       ├── logging.rs  # Structured logging
│   │       └── time.rs     # Time management
│   ├── tests/              # Rust tests
│   │   ├── integration/    # Integration tests
│   │   ├── uci/           # UCI protocol tests
│   │   └── bridge/        # FFI tests
│   ├── benches/           # Benchmarks
│   ├── Cargo.toml         # Dependencies
│   ├── build.rs           # Build script for C++ bindings
│   └── README.md          # Rust-specific documentation
├── cpp/                   # C++ engine core (unchanged)
├── python/                # Python AI wrapper (unchanged)
├── shared/                # Shared resources
└── docs/                  # Documentation

yaml
Copy code

---

## 4. Process & Concurrency Model
- Single process, one stdout writer.
- One active search at a time.
- Cancellation via atomic stop-flag checked by C++ search.
- I/O thread reads stdin, routes commands, triggers Supervisor.
- `stop`, `ponderhit`, `quit` have highest priority.

---

## 5. Parsing & Command Handling
- Efficient hand-rolled tokenizer for hot paths (`position`, `go`, `setoption`).
- Build typed command structs, no raw string passing.
- Router mutates State Model or signals Supervisor.

---

## 6. State Model
- Holds current FEN/moves, UCI options, session flags, time snapshot.
- Immutable snapshot cloned when starting search to avoid race conditions.

---

## 7. Time Management
- Handles `wtime/btime`, increments, `movestogo`, `movetime`, `depth/nodes`.
- Pluggable `TimePolicy` trait (classical/blitz/bullet).
- Safety margin + hard cap enforcement.
- `movetime` treated as strict cap.

---

## 8. UCI Options
- Option registry with type-safe handling (Check, Spin, Combo, String).
- Options validated in Rust, passed to C++ via FFI.
- Custom options:  
  - `MorphyStyle` (Check)  
  - `SacrificeThreshold` (Spin)  
  - `TacticalDepth` (Spin)  

---

## 9. FFI Boundary
- Prefer `cxx` crate for stable interop.
- Pass POD structs or opaque handles.
- Rust owns cancellation tokens, exposed as callback/atomic to C++.
- Functions: `set_position`, `start_search`, `stop`, `clear_tt`, `perft`.

---

## 10. Stop/Cancel Semantics
- Priority: `quit` > `stop`/`ponderhit` > others.
- Guarantee: on `stop`, return `bestmove` quickly (<10ms typical).
- Supervisor signals cancel → C++ search exits → best known move returned.

---

## 11. Output Discipline
- Strict UCI formatting only.
- `info` lines: depth, score, nodes, nps, pv.
- No logging to stdout (use tracing/telemetry instead).
- PV truncation policy for long lines.

---

## 12. Logging & Diagnostics
- Use `tracing` for structured logs (`error`, `warn`, `info`, `debug`, `trace`).
- Hidden `debug uci` commands for runtime diagnostics.
- Optional JSON log output for CI pipelines.

---

## 13. Testing
- Unit: parser property tests, time-control edge cases.
- Integration: transcript tests against UCI scripts.
- Fuzzing: AFL/LibFuzzer on parser.
- Soak tests: repeated `isready/go/stop`.
- Regression: transcript snapshots from GUIs (Arena, CuteChess, Lichess bot).

---

## 14. Performance
- Avoid allocations in hot path, use stack buffers/SmallVec.
- Non-blocking I/O loop.
- Lock-free cancellation flags, no mutexes in stop path.

---

## 15. Error Handling
- Malformed commands: ignore gracefully, never crash.
- Internal recoverable errors: log warning, continue.
- Irrecoverable errors: log, emit `bestmove (none)`, exit cleanly.

---

## 16. Security
- Enforce max line length (e.g., 16k).
- Enforce max tokens (moves in `position`).
- No environment-driven surprises unless explicit.

---

## 17. Packaging
- Single `opera-uci` binary per platform.
- Version stamping (git SHA).
- Engine resources (books, models) found relative to executable or via UCI options.

---

## 18. Developer Ergonomics
- `--selftest` → parser/time smoke tests.
- `--perft` → call into C++ perft for debugging.
- `--bench` → benchmark search speed & stop latency.

---

## 19. UCI Compliance Checklist
- [ ] `uci` → engine id + options + `uciok`
- [ ] `isready` → `readyok`
- [ ] `ucinewgame` → reset state/TT
- [ ] `position [startpos|fen] moves …`
- [ ] `go` variants: depth, nodes, movetime, wtime/btime, infinite, ponder
- [ ] `stop` → cancel + `bestmove`
- [ ] `ponderhit` handled correctly
- [ ] `setoption name … value …`
- [ ] Never output non-UCI text to stdout

---

## 20. Integration Boundaries
- Rust validates input, forwards to C++ cleanly.
- On `go`: Rust sets deadlines, launches C++ search, streams `info`, cancels on time/stop.
- On `ucinewgame`: Rust resets State + calls C++ TT clear.

---

## 21. Morphy-Specific Controls
- UCI options:  
  - `MorphyStyle = true|false`  
  - `SacrificeThreshold = -X..+X`  
  - `TacticalDepth = 0..N`  
- Rust validates, forwards to C++.

---

## 22. CI & Workflow
- `cargo fmt` + `clippy` enforced.
- Unit + transcript tests on every commit.
- Benchmark pipeline for nps, stop latency, bestmove time.
- Binaries packaged with changelog.

---

## 23. Milestones
1. Minimal UCI shell (`uci`, `isready`, `quit`).
2. `position` + `go depth` with dummy C++ stub.
3. Time control + `ponder` + `ponderhit`.
4. Options registry + TT clear + perft passthrough.
5. Telemetry + fuzzing + soak tests.
6. Packaged builds + GUI integration.

---

## 24. Definition of Done
- No panics on malformed input (fuzz >1M cases).
- Stop latency p99 <10ms.
- Full compliance across GUIs.
- Graceful shutdown with no thread leaks.

---

### Final Notes
- Rust is **control-plane only**: parsing, state, concurrency, time mgmt.
- C++ remains the **engine core**.
- Keep FFI boundaries simple and explicit.
- Prioritize predictability, robustness, and clarity over clever tricks.
- Maintain TTD approach with strict 100% code coverage. 