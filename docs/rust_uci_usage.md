# Running Opera through UCI

## Status — 2026-09-18

Opera now has a persistent Rust UCI executable connected to real C++ search. Process acceptance and independent move-legality checks pass on macOS ARM64 and Docker/Linux ARM64. The external python-chess client also passes protocol checks and completes timed Stockfish games. Windows and full Kiro acceptance remain unverified. Engine strength is unfinished; the owner requested [Unity playtesting](unity-integration.md) before further core and neural development in the [roadmap](development-roadmap.md).

The native CI workflow is configured to build and exercise the UCI executable on Linux, macOS and Windows and upload each executable. Windows uses clang-cl with the MSVC Rust target because the C++ core uses GCC/Clang bit-scan intrinsics. Repository Actions is currently disabled, so the pushed candidate has no CI run. Enabling it requires the owner's approval. `rust/Cargo.lock` pins dependencies for the documented `--locked` builds.

## Build and launch

From the repository root:

```bash
cargo build --release --locked --manifest-path rust/Cargo.toml
./rust/target/release/opera-uci
```

Alternatively, `bash launch.sh --uci` builds the release binary and executes it, keeping build output on stderr. Set `CARGO_TARGET_DIR` to an absolute scratch directory to keep build artifacts outside the checkout. Launch the built executable directly from a GUI or game adapter; do not put a compiler invocation in the game's move loop. `launch.sh` without `--uci` still runs the C++ board demo.

On Windows, use the Rust MSVC toolchain and a Visual Studio developer environment with the Windows SDK and clang-cl available. Set `$env:CXX = "clang-cl"` in PowerShell before the Cargo build. The executable is `rust/target/release/opera-uci.exe`; the Bash launch script is optional.

The executable also accepts `--version`, `--hash-size N`, `--threads 1`, `--morphy-style true|false`, and `--debug`. Neural weights and other unsupported arguments fail explicitly. `RUST_LOG=debug` enables diagnostic logs on stderr. Protocol stdout contains only UCI responses in normal engine mode.

## Command lifecycle

Send one command per line and flush stdin. Read stdout continuously:

```text
uci
```

Wait for `uciok` after identification/options, then send `isready` and wait for `readyok`. Start a game and search:

```text
ucinewgame
position startpos moves e2e4
go movetime 100
```

Read `info` updates and wait for exactly one `bestmove` for that search. Its move is for Black in this example. The engine does not apply its own bestmove to the session position; send the complete next position before the next search. Use `position fen <six FEN fields> moves <optional coordinate moves>` for arbitrary positions. Promotion moves include a suffix (`a7a8n`). Castling is king-coordinate notation (`e1g1`).

Supported search limits: `depth`, `nodes`, `movetime`, `wtime/btime`, `winc/binc`, `movestogo`, `mate` (depth bounded at twice the mate target), `searchmoves`, `infinite`, and `ponder`. Mate is a search request, not a guarantee that the engine can find a mate. Depth range is 1–128; mate range 1–64. When depth and mate are combined, the smaller depth bound applies. Infinite/ponder searches honor node and depth limits while withholding bestmove until `stop` (or `ponderhit` for pondering). The pondering clock budget starts at ponderhit.

`searchmoves` requires a nonempty list of legal root moves. Move notation is normalized to lowercase; malformed, illegal or duplicate searchmoves parameters reject the command without replacing the active search. With `setoption name Ponder value true`, a completed search includes `ponder <reply>` when its principal variation contains a reply. The reply is for the position after the reported bestmove; terminal positions still return only `bestmove 0000`.

`isready` remains responsive during search. `stop` cancels and joins the active search and emits its best available move; repeated stop while idle produces no extra bestmove. A replacement go or valid position command stops the prior job before installing the new one. Consume that prior bestmove before associating the next search's result. `quit` and stdin EOF cancel the worker and exit. On Unix, SIGINT and SIGTERM also join the search, drain final output and exit successfully. Windows uses Ctrl+C handling. Output errors stop and join the worker before returning a failure status. A client that leaves stdout unread also triggers bounded failure and runtime shutdown. Checkmate/stalemate return `bestmove 0000`.

Reported scores use the root side's perspective: positive favors the side to move in the supplied position. The C++ evaluator returns White-relative scores; search converts these before negamax propagation.

Invalid FEN/moves preserve the previous position. Unknown/invalid commands report `info string ERROR` and processing continues. Input is bounded to 4096 bytes per line; oversized input is discarded through its newline. FEN input requires two nonadjacent kings, coherent castling/en-passant metadata, no pawns on promotion ranks, and supported counters (fullmove 1–65535, halfmove 0–65535). UCI move lists preserve history for repetition detection during search.

## Advertised options

| Name | Values | Behavior |
| --- | --- | --- |
| Hash | 1–128 MB; default 16 | Sizes the worker's C++ transposition table |
| Threads | 1 only | One search worker; parallel search is not implemented |
| Ponder | true/false; default false | Includes an available PV reply in bestmove; GUI requests pondering using go ponder |
| MorphyStyle | true/false; default false | Selects Morphy activity/king-pressure adjustments or the standard handcrafted evaluator; see [core review](core-engine-review.md) |
| Move Overhead | 0–5000 ms; default 10 | Reserves time from movetime/remaining clock budget |
| Clear Hash | button | Stops/drops the current worker; every search starts with a fresh table |

Option changes affect subsequent searches; invalid values leave settings unchanged. Multiword names accept extra spaces/tabs. Clear Hash takes no value; a trailing `value` without a value is rejected. SacrificeThreshold, TacticalDepth, NN weights, MultiPV, Chess960 and more than one search thread are not advertised or silently simulated. Their underlying features remain deferred.

The active coordinator uses a conservative bounded clock allocation: remaining time divided by movestogo (30 by default), plus 75% of increment, capped by remaining time minus overhead. It uses an atomic deadline checked inside recursive C++ search. The older standalone time-policy module is not on this executable path and still has known failing tests.

## Unity/network adapter boundary

Run this executable as a child process and communicate over redirected stdin/stdout. Keep stderr separate. Serialize each engine instance's position/search sequence; use separate processes for independent games or analysis sessions. The Unity/network layer remains responsible for game state, move application, clocks, draw adjudication and relaying moves. The sibling Unity app now has a local UCI adapter; [package the native executable](unity-integration.md) into its StreamingAssets. This integration needs no network server or account.

## Verification

```bash
python3 scripts/test_uci_process.py /absolute/path/to/opera-uci
python3 scripts/test_uci_oracle.py /absolute/path/to/opera-uci /absolute/path/to/stockfish
cargo test --locked --manifest-path rust/Cargo.toml --lib
cargo test --locked --manifest-path rust/Cargo.toml --test uci_parser_comprehensive --test uci_handshake_integration --test uci_real_search --test search_integration_tests
cargo test --locked --manifest-path rust/Cargo.toml --no-run
ctest --test-dir /absolute/path/to/cpp-build -R '^(SearchEngineTest|SearchControlTest|SearchEvalIntegrationTest)\.' --timeout 5 --output-on-failure
docker build --target uci-validation -t opera-engine:uci-validation .
```

On September 18, all 21 process tests, 217 Rust library tests, 23 search integration tests, 27 parser tests, 11 handshake tests, three real-search tests, and 40 selected C++ search/control/evaluation tests passed. The library suite passes with default parallel execution and serial execution. Both full test targets compile. The process harness covers command-driven startup, black/non-start/terminal positions, invalid-command recovery, combined limits, ponder timing/replies, repeated jobs, bursts, EOF, options, searchmoves, promotions, castling, en passant, capture choice for both colors, Unix signals, closed output and unread-output backpressure. The signal test is skipped on Windows.

The Stockfish oracle independently validates legal bestmoves, every PV prefix and ponder replies, including both colors' special moves. It passed 176 positions on macOS and Linux; the deterministic walk alternates engine and seeded random legal moves with both evaluators. This is a bounded rules check, not universal certification or a strength measurement. The same process/oracle suites passed with AddressSanitizer and UndefinedBehaviorSanitizer applied to the C++ bridge on macOS; Rust itself was not instrumented, and leak detection was disabled.

For external client interoperability, install the small pinned test dependencies in a Python virtual environment:

```bash
python3 -m pip install -r scripts/requirements-uci.txt
python3 scripts/test_uci_client.py /absolute/path/to/opera-uci
# Optionally include two complete timed games and save their PGN:
python3 scripts/test_uci_client.py /absolute/path/to/opera-uci /absolute/path/to/stockfish --pgn acceptance.pgn
```

This uses python-chess's UCI client, exercises actual ponderhit and ponder cancellation, stops infinite analysis, checks restricted special moves and terminal positions, and optionally plays each color against Stockfish with 3-second clocks plus 50ms increment. Stockfish has an additional 20ms move cap to keep this acceptance test short. Games must end within 240 plies with no illegal moves, time forfeits, client parser warnings or abnormal exits. The local macOS games ended by checkmate after 60 and 41 plies; Linux games ended at 54 and 61 plies. All four were losses for Opera. These are interoperability results, not an Elo experiment or a graphical GUI smoke test. The harness has per-operation and whole-run timeouts; CI runs client checks on all three native platforms and timed games on Linux.

September 13 release measurements: 30 isready samples max 0.27ms; 30 stop samples max 0.11ms; 10 quit samples max 1.32ms. Movetime requests 1/10/50/100/250ms completed in about 1.1/1.1/40.1/90.1/240.1ms with the default 10ms overhead. These are historical observations on one machine, not universal timing guarantees. The Kiro task's stricter guaranteed <10ms stop acceptance is not established for every position/platform.

September 18 immediate-cancellation measurements across Hash 1/16/128, both evaluators and starting/Kiwipete positions: 36 stop samples max 13.04ms; 10 quit samples max 3.79ms. These satisfy the observed <50ms stop/<100ms quit targets on this machine, but do not satisfy an unconditional <10ms stop claim.

## Remaining limits

Both test targets compile, but the complete repository test suite is not green. Broad C++ and Rust regression results and remaining failing checks are recorded in the [current baseline](current-baseline.md). The local Unity Mac app passes an initial graphical playtest; that is not general GUI/platform certification. No Elo, 1M NPS, coverage percentage, completed fuzzing campaign or Windows certification is claimed.

Docker's test mode requires handshake/readiness and real search output and returns failure on missing results. Image build, smoke execution and the separate Linux validation target pass locally; they do not execute all repository unit tests.
