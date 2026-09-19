# Opera Chess Engine

A C++ chess search/evaluation core with a Rust UCI executable, intended to support Morphy-inspired play.

## Current status — 2026-09-18

The Rust executable now handles persistent UCI commands and real asynchronous search. It supports positions/FEN/moves, depth/node/time limits, infinite analysis, pondering, cancellation, and supported options. It is ready for basic process-level integration experiments; playing strength and full production acceptance remain unfinished.

```bash
cargo build --release --locked --manifest-path rust/Cargo.toml
./rust/target/release/opera-uci
# Or build and launch through the existing script:
bash launch.sh --uci
```

Use the Rust binary for GUI/game integration. The C++ `opera-engine` executable remains a board demo. See the [UCI usage and Unity adapter boundary](docs/rust_uci_usage.md) for commands, options, supported limits and verification.

Validation on macOS ARM64: 21 process tests, 281 Rust library/search/protocol tests, and 40 C++ search/control/evaluator integration tests pass. Docker/Linux ARM64 also passes the process suite. An independent Stockfish oracle checks bestmoves, every reported PV and ponder replies across 176 positions on both platforms. The C++ bridge also passes these process/oracle suites with address and undefined-behavior sanitizers on macOS. The broader engine suite still has failures; see the [dated baseline](docs/current-baseline.md) for evidence and limitations.

Native UCI CI is configured for Linux, macOS and Windows, including downloadable executables. Windows uses clang-cl for the C++ bridge. Repository Actions is disabled, so native Windows checks remain unverified. An external python-chess client passes protocol checks and timed Stockfish games locally. `uci` is merged into `main`; the existing Unity chess app now supports local games against Opera on this Mac. See [play instructions and packaging](docs/unity-integration.md). Cargo dependencies are pinned in `rust/Cargo.lock`. The [development roadmap](docs/development-roadmap.md) puts hands-on Unity playtesting before further engine-core and neural evaluation/tuning work.

A separate [two-game Stockfish 17.1 match](docs/games/2026-09-18-stockfish-match.md), with equal 30+0.25 clocks and Opera playing each color, ended 0–2 by checkmate on moves 26 and 28. Both games were legal and within their clocks. [Download the PGN](docs/games/2026-09-18-stockfish-match.pgn), including move-by-move clock annotations. This is a development snapshot, not an Elo estimate.

## Docker

```bash
docker build -t opera-engine .
docker run -it opera-engine
docker run opera-engine test
# Run the bounded process suite and independent move oracle in Linux:
docker build --target uci-validation -t opera-engine:uci-validation .
```

The entrypoint starts the Rust binary. `test` checks actual handshake/readiness/search output; it is not the full unit suite. The separate `uci-validation` target runs the process/oracle suites against the runtime binary; its Python/Stockfish dependencies are excluded from the final image. Image build and execution were validated locally through Docker on Linux ARM64. The Dockerfile disables C++ unit tests and does not run Cargo tests. Neural weights remain unsupported.

## Specifications

Kiro requirements/design/tasks live in `.kiro/specs/`. Current implementation evidence is dated separately from historical completion reports. GUI, cross-platform, fuzzing, performance/coverage and advanced style requirements remain open where unverified.
