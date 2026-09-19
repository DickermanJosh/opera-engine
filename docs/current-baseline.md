# Current execution baseline — 2026-09-18

## UCI release-candidate review — 2026-09-18

Release targets are macOS, Windows and Linux. Work remains on `uci`; `main` has not been merged. The [roadmap](development-roadmap.md) puts engine-core correctness and strength next, neural evaluation/training/tuning after that, and the existing Unity project integration later.

This review reproduced and fixed two release-relevant defects. Search passed White-relative evaluator scores directly into negamax, reversing the meaning at alternating depths; both evaluators now convert to the current side's perspective. Tests cover both root colors at odd/even depths and taking an undefended queen. A client that filled stdout without reading could hang process teardown after the async output timeout; the executable now bounds Tokio runtime shutdown after joining the search worker. Logging initialization now uses stderr and returns a configuration error on repeat initialization. Tests that install global logging/panic hooks run in isolated subprocesses.

| Fresh check | Result and scope |
| --- | --- |
| macOS ARM64 release build and process acceptance | Passed, 21/21 tests, including closed and unread stdout |
| Docker/Linux ARM64 runtime build, smoke and process acceptance | Passed; 21/21 process tests |
| Independent Stockfish bestmove/PV/ponder oracle | 176 positions on each platform; macOS 1,452 PV moves/173 ponder replies, Linux 1,533/173 |
| C++ bridge AddressSanitizer + UndefinedBehaviorSanitizer, macOS | Process 21/21 and oracle 176 positions pass; Rust not instrumented, leak detection disabled |
| Rust library | 217/217 pass, both default parallel and serial execution |
| Rust search/parser/handshake/real-search integration | 23/23, 27/27, 11/11, 3/3 pass |
| All compiled Rust tests, each in its own process with a five-second cap | 342/345 pass; three standalone time-policy failures below |
| C++ SearchEngine/SearchControl/SearchEvalIntegration | 40/40 pass |
| Full C++ suite, five-second cap per test | 433/454 pass; 20 assertion failures and one timeout below |
| Local immediate-cancellation samples | 36 stops: maximum 13.04ms; 10 quits: maximum 3.79ms |
| Windows and remote native CI | Configured, not executed/verified here |
| Real GUI/match-manager acceptance | Pending |

The full C++ run retains failures in `MemoryAuditTest.ComprehensiveMemoryBenchmark`; six `AlphaBetaTest` checks (`DepthZeroSearch`, `DepthTwoSearch`, `AvoidMateInOne`, `EndgamePosition`, `MaxDepthHandling` timeout, `SearchEfficiency`); `SearchOptimizationTest.OptimizationMethods`; four advanced evaluation checks (`AdvancedPassedPawnBonusScales`, `KingSafetyPhaseDependent`, `BishopMobilityBonus`, `PhaseDependentKingEvaluation`); three performance checks (`MoveGenerationSpeed`, `ComplexPositionMoveGen`, `SearchNodeScaling`); and six Morphy checks (`KingSafetyEmphasis`, `SacrificeCompensation`, `UncastledKingPenalty`, `InitiativeValuation`, `CentralControlEmphasis`, `BiasScaling`). Performance assertions depend on build instrumentation and load; this result records failures rather than diagnosing every one as an engine defect. The tactical suite passed this run after the perspective fix; no playing-strength estimate follows from that.

The remaining Rust failures are `test_standard_policy_full_game_simulation`, `test_standard_policy_movestogo_sudden_death`, and `test_standard_policy_time_trouble`. This legacy policy is not on the active UCI executable path. Its implementation and expectations need reconciliation during core work. The formerly failing depth/node scaling integration test passes with the perspective fix.

One timing assertion was corrected: shallow search may finish within a millisecond, so its info callback must have a consistent zero-time NPS value rather than invent a positive elapsed time. Core tactical/style expectations were not weakened to make the suite pass. The stricter historical <10ms stop guarantee, full coverage/fuzzing, strength, advanced options and complete Kiro acceptance remain open. Platform and GUI checks still prevent calling this candidate production-ready across all three requested systems.

Reproducible process/oracle/CI commands are in [the usage guide](rust_uci_usage.md). Local evidence: `/tmp/opera-uci-process-final.log`, `/tmp/opera-uci-core-tests.log`, `/tmp/opera-uci-lib-parallel.log`, `/tmp/opera-uci-search-final.log`, `/tmp/opera-uci-cpp-final-broad.log`, `/tmp/opera-uci-rust-final-regressions.json`, `/tmp/opera-uci-docker-validation.log`, `/tmp/opera-uci-sanitized-process.log`, `/tmp/opera-uci-sanitized-oracle.log`, and `/tmp/opera-uci-latency.json`. These are local scratch logs, not repository dependencies. The C++ CTest build is `/tmp/opera-uci-cpp`.

For the sanitizer run, the bridge was compiled with `CXXFLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer'`, an explicit `aarch64-apple-darwin` Cargo target, and clang linking the ASan runtime reported by `clang -print-file-name=libclang_rt.asan_osx_dynamic.dylib` with its parent directory as an rpath. `ASAN_OPTIONS=halt_on_error=1:detect_leaks=0` and `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1` were set while running both harnesses. This describes the C++ checks performed, not whole-program Rust instrumentation or a leak-freedom claim.

## Earlier UCI reliability and native CI follow-up — 2026-09-18

Continued the existing uncommitted UCI implementation on macOS ARM64, using Rust 1.78.0 and the cached locked dependencies. New process regressions first reproduced six missing behaviors: infinite search bypassing node limits; depth overriding the mate bound; uppercase searchmoves rejected; multiword option whitespace rejected; missing ponder replies; and abrupt SIGINT/SIGTERM termination.

The coordinator now uses parsed, normalized searchmoves with transactional validation, applies the tighter depth/mate bound, and preserves node limits in infinite/ponder modes. Enabling Ponder adds a reply from the completed PV when available. The ponderhit deadline is published before the worker leaves ponder mode. Button values are validated. Signal and I/O failure exits stop and join the worker; output writes/flushes and final draining use the configured timeouts.

Build follow-up: added the existing Cargo lockfile to versioned project content; switched bridge compiler options to cc's target-aware C++17/optimization settings; configured Linux, macOS and Windows native CI, with clang-cl on Windows; removed the Docker CI step that treated startup failures as success. Remote CI and Docker execution were not run here.

Fresh verification:

| Check | Result |
| --- | --- |
| Locked offline release build | Passed; existing repository warnings remain |
| Bounded process harness | 19/19 passed, including Unix signals and a closed output pipe |
| Rust UCI library tests (`--lib uci::`) | 109/109 passed |
| Parser / handshake / real-search integration | 27/27, 11/11, 3/3 passed |
| All Rust test targets (`--no-run`) | Compiled |
| Full C++ Release test executable | Compiled |
| C++ SearchEngine/SearchControl tests, five-second per-test cap | 28/28 passed |
| Changed Rust formatting, workflow YAML parsing, diff whitespace | Passed |

Reproduction commands are in [the usage guide](rust_uci_usage.md). Local build/test logs are `/tmp/opera-uci-release-build.log`, `/tmp/opera-uci-test-build.log`, `/tmp/opera-uci-components.log`, `/tmp/opera-uci-integration.log`, and `/tmp/opera-uci-cpp-build.log`; the CTest build is `/tmp/opera-uci-cpp`.

Remaining: execute the new native CI matrix and a real GUI/game-adapter smoke test; resolve the previously recorded broad engine/time-policy failures; implement deferred style/multithreading options where needed; measure performance/coverage and run fuzzing/sanitizers. The broad September 13 suite was not rerun or relabeled as passing. Existing Kiro acceptance boxes that require those results remain open.

## UCI implementation follow-up — 2026-09-13

The September 7 assessment below is preserved as history. The current Rust executable is now connected to real C++ search; see [usage and integration guide](rust_uci_usage.md). No Unity/network, neural, or evaluation-strength expansion was performed.

Implemented: persistent command-driven I/O; transactional complete FEN/moves; special-move legality; worker-owned board/search state and independent atomic cancellation; bounded time/node/depth search; ponder/infinite lifecycle; real info/PV/bestmove; supported options; clean stdout; launch and container-smoke integration. Search fixes include inner-loop limits, destination-square extension logic, bounded extensions, repetition/check-evasion handling, and promotion-preserving moves.

Observed verification: release and debug builds; 11/11 external-process tests; 28/28 C++ SearchEngine/SearchControl tests; both game/PV and special-board-state integration tests. Full C++ target compiles; broad run 426/452 pass with a five-second cap (26 assertion failures/timeouts). Full Rust test targets compile; 338/342 bounded Rust tests pass (including the two real-search integration tests); remaining broad failures concern a depth/node-scaling assertion and three legacy standalone time-policy expectations. These are not hidden by the UCI acceptance tests. GUI, cross-platform, Docker, sanitizer, full coverage, Elo/style and throughput targets remain open.

Test corrections address actual obsolete APIs, unsafe shared raw-pointer tests, tests located outside their private module, SAN mistakenly used as UCI, assertions accepting illegal pawn moves, the broken Clear Hash parse, and a checkmate assertion that demanded a legal move. The C++ checkmate/stalemate tests now assert null move and score. Original historical reports are not reclassified as current passes.

Evidence: local task `work/uci-release-tests.log`, `uci-search-tests.log`, `uci-game-tests.log`, `cpp-tests-final.log`, `rust-regressions-final.json`, and `uci-timing.json`. New repository tests live in `scripts/test_uci_process.py` and `rust/tests/uci_real_search.rs`. Test-target compilation command: `cargo test --manifest-path rust/Cargo.toml --locked --offline --no-run`; deep regressions were run in separate processes with five-second limits so one slow test could not block the remainder.

Next: integrate through the documented process boundary in the user's Unity/game adapter, verify target operating systems/GUI behavior, then resolve remaining engine correctness/quality and performance tests before stronger production claims. Kiro requirements remain intact; unchecked validation, unsupported advanced options, and deferred tuning are not declared complete.

## Historical September 7 assessment

Audited branch `uci`, commit `2f9788f`, on macOS ARM64. This date uses America/Los_Angeles. The repository was clean before the audit. This update changes documentation only; no engine, test, build-script, requirement, or design implementation was changed.

## Observed results

| Check | Result | Scope / limitation |
| --- | --- | --- |
| Rust default-feature build | Passed, with warnings | Cargo/rustc 1.78.0; `--locked --offline`; default `ffi` compiles C++ sources via build.rs; debug profile only |
| C++ Release core, demo, perft | Built | Apple Clang 21, CMake 3.24.4; core archive and two executables produced |
| Full C++ test build | Compilation failure | Missing `std::setprecision` declaration in TacticalEPDTest.cpp:455 and MorphyStyleValidationTest.cpp:372–374; no full-suite result |
| Full Rust test command | Compilation failure | basic_functionality_test uses absent RuntimeConfig::production and RuntimeManager::get_runtime; integration_ffi_test has 16 unresolved FFI references |
| Rust search integration target | Compilation failure | Unsafe block denied; raw `*mut SearchEngine` cannot be sent between threads (lines 159–160) |
| Rust library tests | 204 passed, 0 failed | `cargo test --lib`, default FFI; 4.94 seconds; not a whole-project test pass or coverage measurement |
| C++ selected actual search tests | Five passed | Existing SearchEngineTest.cpp compiled separately against freshly built core: starting position, depth limit, node limit, checkmate, stalemate |
| C++ time-limited search | External timeout at 20 seconds | SearchRespectsTimeLimit requests 100ms and asserts ≤150ms; it did not return before external timeout; cause not diagnosed |
| Full perft suite | External timeout at 45 seconds | No completed suite result; large default depths, so this alone does not establish a hang |
| Targeted perft | Expected counts reproduced | Start position depth 1–4: 20, 400, 8,902, 197,281; three previously disputed positions at depth 5: 674,624; 185,429; 135,655 |
| Docker | Environment blocker | Client 28.5.1 installed; daemon unavailable; no image build/run validation |

Google Test 1.17.0 was available under `/opt/homebrew`; Ninja was not on PATH, so CMake's default Makefiles generator was used. No dependency installation or source workaround was needed for the successful builds. Full-suite compile failures are source/API failures on this toolchain, not missing dependencies. Rust release and Linux/Docker builds were not verified.

## Runtime entrypoints and integration

The freshly built Rust `opera-uci` produced the same stdout for empty input and for `uci`, `isready`, `position startpos`, `go depth 2`, `quit`:

```text
Setting hash size to 128 MB
Setting thread count to 1
id name Opera Engine
id author Opera Engine Team
uciok
```

Both runs exited successfully after approximately one second, with no `readyok` or `bestmove`. `rust/src/main.rs` contains TODOs for the command processor and I/O loop, then prints identification and sleeps. A successful exit or the presence of `uciok` therefore does not demonstrate UCI compliance. The settings messages also occupy protocol stdout.

`rust/src/uci/engine.rs` separately leaves position/new-game/stop C++ integration as TODOs. Its go handler waits 100ms, records 1,000 simulated nodes, and sends hard-coded `bestmove e2e4`. Wiring this handler into main alone would still not yield real chess search. Other handlers and FFI/search modules exist, and `cpp/src/UCIBridge.cpp` calls the real C++ search engine, but the executable path does not connect them. Hash/thread/clear-hash setters in that bridge also contain TODOs.

The freshly built C++ `opera-engine` ignores the same UCI command input, prints a board, demonstrates e2e4, and exits. `launch.sh` starts this demo; it is not a standalone C++ UCI engine. The selected search tests establish that actual search code executes, not that either executable can play through a GUI.

The Dockerfile sets `BUILD_TESTS=OFF` and runs `cargo build --release --features ffi`, with no test commands. Contrary to historical entrypoint comments, full unit tests are not run during image construction. Its runtime `test` mode checks identification/process exit and can print success despite missing real command handling; it is not a full test suite. NN weights, CLI settings, image size, and cross-platform acceptance remain unverified.

## Historical claims

`test-failure-diagnosis.md` and `perft-test-case-issue.md` are reports dated 2025-11-29, not current validation. Current PerftRunner.cpp already contains the three corrected expectations, and this audit reproduced those values. No independent Stockfish run was performed; reproducing four positions does not prove universal move-generation correctness or a 19/19 suite pass.

Historical NPS, evaluation latency, coverage, tactical/style, production-readiness and stop-time claims are not revalidated here. In particular, an observed stop time below 50ms does not establish a below-10ms requirement, and a below-10ms startup observation does not establish below 1ms. The present time-limit timeout needs investigation before time-control acceptance.

## Ordered next milestones

1. Restore compilation of the full C++ and Rust test suites by resolving test headers/API drift and safe search-test coordination. Preserve intended test assertions; do not erase failures or relax requirements to get green results. Run bounded tests and record actual failures separately from build errors.
2. Diagnose search time-limit/cancellation behavior. Require externally bounded tests for time, node, depth, stop, and terminal positions, with measured response latency against the actual specification. Do not infer a guaranteed bound from a looser test threshold.
3. Complete a minimal playable Rust UCI executable: persistent stdin/event loop; command-driven handshake; actual board updates; asynchronous real C++ search; stop and quit; legal position-dependent bestmove. Test both sides to move and terminal positions so a fixed e2e4 cannot pass. This is the first usable-engine milestone.
4. Revalidate Kiro acceptance with executable transcripts, full suites and a GUI smoke test, then address advanced options, Docker/release/platform verification, tactical/style tuning and measured performance/coverage. Keep requirements/design as intent until evidence supports acceptance.

## Reproduction and evidence

Audit logs and scratch artifacts are in `/Users/josh/Documents/Codex/2026-09-07/assess-whether-the-existing-chess-engine/work/` (local audit evidence, not tracked repository dependencies): `rust-build.log`, `cpp-build.log`, `rust-tests.log`, `rust-search.log`, `rust-lib.log`, `uci-commands.log`, `uci-empty.log`, `cpp-demo.log`, `perft.log`, `perft-0.log` through `perft-3.log`, `real-search.log`, and separately named terminal/node search-test logs. `real-search.log` records the two passes followed by the timed-out time-limit test.

Use an external process timeout when reproducing tests. Equivalent build commands, with a writable scratch directory:

```bash
REPO='/Users/josh/dev/Actice Projects/opera-engine'
AUDIT_OUT="$(mktemp -d /tmp/opera-audit.XXXXXX)"
cmake -S "$REPO/cpp" -B "$AUDIT_OUT/cpp" -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF
cmake --build "$AUDIT_OUT/cpp" -j 4
cargo build --manifest-path "$REPO/rust/Cargo.toml" --target-dir "$AUDIT_OUT/rust" --locked --offline
cargo test --manifest-path "$REPO/rust/Cargo.toml" --target-dir "$AUDIT_OUT/rust" --locked --offline --lib -- --test-threads=1
cmake -S "$REPO/cpp" -B "$AUDIT_OUT/cpp" -DBUILD_TESTS=ON -DGTest_DIR=/opt/homebrew/lib/cmake/GTest
cmake --build "$AUDIT_OUT/cpp" -j 4
cargo test --manifest-path "$REPO/rust/Cargo.toml" --target-dir "$AUDIT_OUT/rust" --locked --offline
cargo test --manifest-path "$REPO/rust/Cargo.toml" --target-dir "$AUDIT_OUT/rust" --locked --offline --test search_integration_tests
printf 'uci\nisready\nposition startpos\ngo depth 2\nquit\n' | "$AUDIT_OUT/rust/debug/opera-uci"
"$AUDIT_OUT/cpp/perft-runner" 'rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1' 4
```

Independent C++ search test build (unchanged existing source; this bypasses unrelated full-target compile failures and is not a full suite build):

```bash
clang++ -std=c++17 -I "$REPO/cpp/include" -I /opt/homebrew/include   "$REPO/cpp/tests/SearchEngineTest.cpp" "$AUDIT_OUT/cpp/libopera_core.a"   /opt/homebrew/lib/libgtest.a /opt/homebrew/lib/libgtest_main.a -pthread -o "$AUDIT_OUT/search-tests"
# Run each test with a bounded subprocess wrapper; e.g. Python subprocess.run(..., timeout=20).
"$AUDIT_OUT/search-tests" --gtest_filter=SearchEngineTest.BasicSearchFromStartingPosition
```

Offline Cargo requires cached dependencies; other machines may need dependency downloads. Homebrew GTest paths are specific to this audit machine.
