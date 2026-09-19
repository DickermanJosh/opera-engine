# Development roadmap — 2026-09-18

Target operating systems: macOS, Windows and Linux. Finish the engine core before building and tuning neural evaluation. Integrate the existing Unity chess project later, through the UCI process boundary.

## 1. Close the UCI release gate

The implemented protocol is a release candidate. macOS ARM64 and Docker/Linux ARM64 pass bounded process acceptance and independent Stockfish legality checks. Remaining release work:

- Enable repository Actions with the owner's approval, then run the native CI matrix, especially the Windows MSVC/clang-cl build and executable tests; confirm the uploaded binaries work. The pushed candidate currently has no checks because GitHub reports Actions disabled.
- Retain external-client acceptance: python-chess now exercises analysis/stop, actual ponder hits and cancelled pondering, special moves and complete timed games against Stockfish. A graphical GUI smoke test remains open for broader compatibility coverage.
- Resolve any protocol/platform defects found and retain reproducible regressions. Record the platform and architecture tested, without treating configured CI as a pass.
- Commit the reviewed candidate, merge `uci` into `main` only after release acceptance, and create `engine-core` from the merged main branch. Keep the existing search/evaluation branches intact.

UCI release acceptance does not claim a strong or complete chess engine. The known broad engine failures remain visible in the [baseline](current-baseline.md). The stricter historical timing, coverage and performance targets remain open until measured.

## 2. Finish core correctness and establish strength

Start on `engine-core` with bounded, reproducible failing positions. The latest UCI review fixed an evaluator-perspective error: both evaluators report White-relative scores, while negamax requires the current side's perspective. Keep odd/even-depth and both-color capture regressions in the release gate.

Prioritize:

1. Audit board restoration, hashing, repetition and draw handling, quiescence terminal positions, check evasions, and mate distance/score propagation. Compare move generation against independent perft/reference positions.
2. Audit transposition-table score/bound handling and replacement, aspiration-window completion, move ordering, extensions and pruning. Establish a simple correct search baseline before optimizing it.
3. Audit piece-square orientation, tapered evaluation, material/tempo perspective, pawn structure and king safety. Reconcile remaining tactical/style tests with valid positions and stated score conventions.
4. Resolve the legacy time-policy failures and decide whether to remove the unused path or adopt it behind the active coordinator's tested deadline contract.
5. Establish repeatable fixed-node tactical results, timed games and a strength baseline with saved openings, seeds, opponents and hardware. Measure changes through paired matches; node-count floors alone do not establish search quality.

Completion evidence: correctness regressions pass; all remaining suite failures are fixed or individually justified and replaced with meaningful checks; complete timed matches remain legal and within budget; strength and speed results are recorded. Defer parallel search and optional style controls until they serve a measured need.

## 3. Build neural evaluation, then tune it

Choose the architecture and inference budget after profiling the stable core. Define features, score perspective, training labels and export format before collecting data. Build reproducible data generation with deduplication and separated training/validation/test sets. Implement a reference inference path, then C++ inference with export/parity tests and portable fallback code for the three operating systems.

Train a baseline model, evaluate it with the existing tactical/match harness, and tune search and evaluation using held-out evidence. Version model metadata and weights; verify loading errors, deterministic inference, memory use and latency. Do not advertise a neural option until real weights and inference are connected and tested.

## 4. Integrate Unity

Inspect the user's existing Unity project when this milestone begins. Launch the platform's packaged UCI executable as a child process; keep protocol stdout and diagnostic stderr separate. The game owns clocks, legal game state, draw adjudication and move application. Test new games, promotion, pondering, cancellation, process crashes and clean shutdown on each supported OS. Networking and distribution decisions follow the actual Unity project's requirements.
