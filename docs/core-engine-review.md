# Core engine review — 2026-09-19

Work is on `engine-core`, based on the Unity integration baseline `2b4353b`. The goal is sound, active play: development, central activity, coordinated attacks and justified sacrifices, with sensible king safety. This is a handcrafted engine; no neural model or learned style is connected yet.

## Changes that affect play

- Corrected the reversed pawn tables. Starting-rank pawns no longer receive the rewards intended for advanced pawns. Central development and promotion progress now have the intended orientation for both colours.
- King shelter depends on pawns **ahead of the actual king**, with a cost for exposure and abandoning castling rights in the centre. Both castling wings are recognized. Safety fades with material, preserving endgame king activity.
- Mobility now measures available squares, excluding friendly occupancy and enemy pawn attacks. Bishop coordination and pressure around the enemy king contribute to activity. Morphy adjustments reuse those components, compare initiative against the opponent and bound material compensation; losing a pawn alone earns no bonus.
- Replaced the exchange evaluator with an occupancy-aware swap calculation. It reveals sliding attackers, excludes pinned/illegal king recaptures, handles en passant and promotions, and lets either side decline an unfavourable exchange. Checking sacrifices and promotions are retained in quiescence even when immediate material exchange is negative.
- Quiescence recognizes stalemate, quiet promotions, check evasions and insufficient material. Mate takes precedence over the fifty-move claim; the rule now uses 100 half-moves. Repetition keys ignore unusable en passant rights.
- Hash entries preserve complete move identity and full position keys. Replacement favours retaining recent, deep entries. Mate scores account for their distance from the root, and score reuse checks the reversible-move clock. Evaluator changes clear cached scores; restricted-root results are not cached as unrestricted results. Aspiration searches widen to a complete window when necessary.
- Search feeds successful quiet moves into the ordering tables it actually uses. Late-move re-searches avoid duplicate work. Null-move pruning now saves/restores real state, disallows consecutive passes and pawn/low-material endings, and verifies deeper cutoffs. Shallow razoring and futility pruning retain check/PV safeguards.

Castling is not compulsory. In the paired Queen's Gambit game as White, `10.Kd1` answers `...Bb4+`: castling is illegal in that position. A separate regression compares safe castling against unforced king walks in a quiet opening. Another requires Morphy's Opera Game finish, `Qb8+! Nxb8 Rd8#`.

## Search cost

Removed Board/history copies from generated-move legality checks, made piece hashing incremental, precomputed king/knight attacks, and replaced the per-node move-score hash map with fixed storage and one hash lookup per list. Morphy evaluation reuses the base evaluator's computed components. Node counting now counts horizon nodes once, so old/new NPS figures are not directly comparable.

On this Mac ARM64 machine, ten fixed positions at depth six, one thread and 16 MiB hash:

| Evaluator | Baseline total of median times | Updated total | Speedup |
| --- | ---: | ---: | ---: |
| Handcrafted | 1,265 ms | 560 ms | 2.26× |
| Morphy | 1,603 ms | 786 ms | 2.04× |

The baseline uses two repetitions for handcrafted and three for Morphy; the candidate uses three for both. These include opening, London, tactical, promotion and endgame positions. Selective search and evaluation changed, so this measures time to the requested depth, not identical search trees or a universal speed guarantee. Raw timings, moves, FENs and binary hashes are in [the benchmark report](benchmarks/2026-09-19-core-search.json). Reproduce with `scripts/benchmark_core.py` and the pinned Python dependencies in `scripts/requirements-uci.txt`.

## Paired games

The new engine with Morphy style beat the saved default engine **8 wins, 0 draws, 0 losses**. Four fixed eight-ply openings were played with colours reversed: Open Game, London, Queen's Gambit and Sicilian. Both engines received 100 ms per move, one thread and 16 MiB hash, with fresh game state for each game. All eight ended in checkmate; no game hit the 240-ply continuation cap. Every move and emitted PV was checked by python-chess.

Read the [games](games/2026-09-19-core-match.pgn) and [results/settings/binary hashes](benchmarks/2026-09-19-core-match.json). `scripts/match_core.py` reproduces the procedure. This is a small regression match, not an Elo estimate or proof of a historical playing style. Both the engine improvements and the style selection differ from the old app. Opera still lost both short client-acceptance games against Stockfish.

## Validation

- C++ Release: **474/474** tests pass, including 20 new core regressions. Thirteen of the initial fourteen regressions failed against the original implementation before fixes.
- Rust: **346** library/integration tests pass, plus **16** documentation examples. Legacy time-policy expectations were corrected, and the dormant policy now selects the proper clock, guards zero moves-to-go and keeps the soft budget within the hard budget. The active UCI coordinator's separate deadline contract remains in use.
- macOS process acceptance: **21/21**. Independent Stockfish oracle: **1,016 positions**, **11,141 PV moves**, **1,011 ponder replies**. Python-chess acceptance covers stop/ponder/restart, special moves and two complete timed Stockfish games.
- C++ AddressSanitizer/UndefinedBehaviorSanitizer: **68/68** selected board/rule/perft/core checks pass. Leak detection was disabled; this is not whole-program Rust instrumentation.
- Docker/Linux ARM64: release build and process/oracle/client acceptance pass. Windows execution remains unverified; configured CI is not a substitute for running it.
- Unity: the existing 494 independent board/FEN fixtures, perft checks and real C# client move/cancellation/restart/process-cleanup checks pass with this engine. The client now selects Morphy style; UCI's general default remains handcrafted.

Older tests were corrected where their fixtures or assertions contradicted chess or the public API: forward pawn captures, a kingless promotion position, unequal-material comparisons labelled equal, adjacent kings in a mate fixture, White-relative score signs, and a 50-half-move draw. Performance tests no longer round sub-millisecond searches to zero NPS or demand that improved pruning visit a minimum number of nodes. Coverage is opt-in, so Release benchmarks execute Release code. The transposition table remains owned by one search worker; a former concurrent-write test introduced a data race and now checks independent worker tables.

Local logs use `/tmp/opera-core-*`; source regressions and recorded benchmark/match artifacts are versioned. Longer matches, held-out tactical tests and user playtesting should guide further search/evaluation tuning before neural work. Windows/native packaging acceptance, parallel search, neural evaluation/training and full coverage/fuzzing targets remain future work.
