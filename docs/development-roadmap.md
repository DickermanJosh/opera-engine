# Development roadmap — 2026-09-19

Target operating systems: macOS, Windows and Linux. The Unity playtest is connected. Current work on `engine-core` addresses sound, active Morphy-inspired play and search speed before building and tuning neural evaluation. See the [core review and recorded evidence](core-engine-review.md).

## 1. Play the current engine in Unity

The owner authorized merging `uci` into `main`; merge `bde03c3` is pushed to `origin/main`. Integration work is on `unity-integration` in this repository and `opera-integration` in the sibling `chess` repository. Keep search and evaluation unchanged during this milestone.

Connect the existing board through an owned UCI child process with background pipe readers, selectable human color and move time, promotion selection, game-end display and reliable restart/shutdown. Check the app's board rules against independent positions, then build and playtest the local macOS ARM64 app. See [Unity setup](unity-integration.md).

## 2. Finish platform acceptance

The implemented protocol is a release candidate. macOS ARM64 and Docker/Linux ARM64 pass bounded process acceptance and independent Stockfish legality checks. Remaining release work:

- Enable repository Actions with the owner's approval, then run the native CI matrix, especially the Windows MSVC/clang-cl build and executable tests; confirm the uploaded binaries work. The pushed candidate currently has no checks because GitHub reports Actions disabled.
- Retain external-client acceptance: python-chess now exercises analysis/stop, actual ponder hits and cancelled pondering, special moves and complete timed games against Stockfish. A graphical GUI smoke test remains open for broader compatibility coverage.
- Resolve any protocol/platform defects found and retain reproducible regressions. Record the platform and architecture tested, without treating configured CI as a pass.
- Keep the existing search/evaluation branches intact. Merging the candidate at the owner's request does not substitute for the remaining Windows and platform acceptance checks.

UCI release acceptance does not claim a strong or complete chess engine. The known broad engine failures remain visible in the [baseline](current-baseline.md). The stricter historical timing, coverage and performance targets remain open until measured.

## 3. Finish core correctness and establish strength

The first core pass is implemented on `engine-core`: the full C++/Rust suites pass, the king/pawn evaluation and tactical search defects have reproducible regressions, and benchmark/paired-game results are recorded. Continue with broader held-out tactics, longer paired matches and user feedback before tuning weights or adding more pruning. The earlier UCI review fixed evaluator perspective; retain those odd/even-depth and both-colour capture regressions.

Prioritize:

1. Audit board restoration, hashing, repetition and draw handling, quiescence terminal positions, check evasions, and mate distance/score propagation. Compare move generation against independent perft/reference positions.
2. Audit transposition-table score/bound handling and replacement, aspiration-window completion, move ordering, extensions and pruning. Establish a simple correct search baseline before optimizing it.
3. Audit piece-square orientation, tapered evaluation, material/tempo perspective, pawn structure and king safety. Reconcile remaining tactical/style tests with valid positions and stated score conventions.
4. Legacy time-policy failures are resolved and clock/limit edge cases covered. Keep the active coordinator's tested deadline contract; the standalone policy remains separate.
5. Establish repeatable fixed-node tactical results, timed games and a strength baseline with saved openings, seeds, opponents and hardware. Measure changes through paired matches; node-count floors alone do not establish search quality.

Completion evidence: correctness regressions pass; all remaining suite failures are fixed or individually justified and replaced with meaningful checks; complete timed matches remain legal and within budget; strength and speed results are recorded. Defer parallel search and optional style controls until they serve a measured need.

## 4. Build neural evaluation, then tune it

Choose the architecture and inference budget after profiling the stable core. Define features, score perspective, training labels and export format before collecting data. Build reproducible data generation with deduplication and separated training/validation/test sets. Implement a reference inference path, then C++ inference with export/parity tests and portable fallback code for the three operating systems.

Train a baseline model, evaluate it with the existing tactical/match harness, and tune search and evaluation using held-out evidence. Version model metadata and weights; verify loading errors, deterministic inference, memory use and latency. Do not advertise a neural option until real weights and inference are connected and tested.

## 5. Expand Unity packaging

After the local playtest, build matching native binaries and test the complete app/engine pair on Windows and Linux, plus any additional Mac architecture. The desktop resolver supports those package paths, but only executed platform checks count as acceptance. The app owns legal game state, draw adjudication and move application. Online transport, match clocks, pondering and distribution remain separate follow-ups.
