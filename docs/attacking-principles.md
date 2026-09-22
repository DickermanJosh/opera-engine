# Attacking principles — 2026-09-21

This pass implements the first core changes from the [Morphy and opponent-model audit](morphy-opponent-style-review.md). Its aim is useful tempo: involve another piece, constrain the defense, retain the pieces needed for the attack, and calculate the finish. No opponent model or neural evaluator is introduced.

## Changes

- **Usable attackers and defenders.** One shared attack map now serves mobility, king danger and material threats. Absolute pins restrict a piece's useful moves and defensive contribution. A pinned slider can still move along its pin and capture the pinner. Geometric attacks remain separate because even a pinned piece denies squares to an enemy king.
- **Mating coordination.** King danger considers coordinated attackers, checking access, weak squares and escape space. It includes a second rank in front of the king to recognize rook invasion/lifts. The large nonlinear reward requires multiple attackers, at most one estimated king flight, and enemy control of available escape squares. Enemy control of every available escape receives extra weight. Shelter and ordinary checking potential fade with material phase; a constrained net can still matter after unrelated pieces have been exchanged. A checking slider's ray extends through the defending king's vacated square when estimating flights.
- **Rook participation and threats.** Rooks receive modest credit for contesting an open file and useful activity on the seventh rank. The seventh-rank bonus excludes squares attacked by enemy pawns, but does not prove tactical safety. Material threats can earn a useful tempo without giving check. The threat term pays once per attacked piece and caps the total at 120 cp; chasing pawns receives no extra credit here. Existing whole-army development and bishop-readiness terms remain intact.
- **Forcing search.** Quiescence includes quiet checks at its entry, then continues captures and required check evasions. The extra allowance is consumed after any move, preventing its repeated renewal during a long exchange sequence. Existing protection for checking sacrifices and promotions remains. Quiet checks receive early ordering priority at plies 0–2; a small quiet-move seed favors castling, new minor development, and central pawn moves that release bishops. These ordering priorities do not add to evaluation.
- **Constant board state.** `Board::givesCheck` predicts direct, discovered, castling, en-passant and promotion checks without changing the board or history. It accepts generated moves; legality of the moving side's king is checked when the move is made.

The maps are positional estimates, not a replacement for legal move generation or tactical search. Relative pins, overloads, clearance sacrifices and unique defensive roles beyond absolute pins still depend primarily on calculation. Nonchecking negative-SEE captures remain selectively pruned in quiescence; this pass does not claim to solve every quiet sacrifice or defensive resource. Morphy's existing multipliers and sacrifice-compensation constants were not increased.

## Concrete decisions

Cold three-second searches, Morphy style, one thread and 16 MB hash, with the same PGN history as the original audit:

| Position | Earlier packaged engine | Revised engine | Interpretation |
| --- | --- | --- | --- |
| 2700-target game, move 12 | Bc4 | **Bb3** | Retains the useful bishop. |
| 2700-target game, move 19 | bxa6 | **Bxf7+** | Chooses a sound active sacrifice instead of collecting another pawn. A fresh four-million-node comparison gives Bxf7+ +0.93 and bxa6 0.00; the rook moves remain stronger at about +2.3–2.4. The earlier development candidates chose Rfd1, but this is not the final choice. |
| 2103-target game, move 20 | Qxf6 | **Nb5+** | Keeps the attack alive instead of exchanging queens; agrees with the reference's preferred continuation. |
| Earlier user game, move 15 | Qd5+, mate in three | **a3!, mate in two** | Quiet preparation is faster than the immediate check. Every one of Black's 16 legal replies permits mate on White's next move. The engine and its color-reflected regression find this at shallow depth. |
| 2700-target game, moves 29–30 | About +3.80 for White | +1.89 / +1.53 | Still too optimistic about a losing position. The revised choices Qh8+ and Rad1 are not successful defenses; the earlier deep reference judged Rad1 worse than Ne2. |

The last row is a remaining weakness, not an accepted practical gamble. The king-safety component now recognizes White's greater danger, but the complete evaluation and finite search still overrate its material and chances. Best-defense reliability needs more work before opponent-dependent risk can be meaningful.

[Saved diagnostics](benchmarks/2026-09-21-initiative-critical.json) include full legal variations, FENs, game/ply identifiers, scores, time limits and the candidate hash. The reference analysis is retained in the [pre-change audit data](benchmarks/2026-09-21-bot-game-critical.json). Scores from the two engines are not calibrated to a common numerical scale.

## Calibration and games

Increasing attack rewards indiscriminately did not strengthen the engine. The first full candidate scored **5 wins, 3 draws and 8 losses** in sixteen paired 100 ms games. An earlier evaluation-only diagnostic scored **4 wins, 4 draws and 8 losses**. That diagnostic predates the final ordering changes and still rewards undefended pawn targets by 8 cp, so it is not a perfectly isolated one-variable ablation.

The initial candidate's overly large quadratic danger term could overvalue potential checks. Independent reference probes found two tempting sacrifices weaker than available alternatives: Bxf7+ instead of Nxc5 in the open game, and Qc2 allowing the bishop on e5 to fall in the queen's-gambit game. The revised engine chooses Nxc5 and Nd2 in fresh three-second checks. Nd2 is a viable reference continuation, though the reference prefers Bg3.

Reducing the nonlinear danger term from `min(600, units² / 16)` to `min(400, units² / 32)`, with a separate 24 cp constraint when multiple attackers deny every available king flight, improved the development sample to **7 wins, 4 draws and 5 losses**. However, that candidate then scored **0 wins, 4 draws and 4 losses** on eight new opening/color combinations at 250 ms. It was not packaged. The same new search with the original evaluation scored **4 wins, 4 draws and 0 losses** on those eight starts, pointing toward the new evaluation rather than search alone as the regression's cause.

The final version distinguishes checking potential from a constrained net. Ordinary pressure fades with phase, and the nonlinear term is gated by actual escape restriction. This leaves room for endgame king activity while preserving the thin-material mating-net regression. It scored **3 wins, 3 draws and 2 losses** on the previously troublesome English, Reti, Scotch and King's Indian starts. A further set using previously unused Catalan, Dutch, Scandinavian and Alekhine starts ended **2 wins, 5 draws and 1 loss**. Both final sets used 250 ms per move, colors reversed, for **5 wins, 8 draws and 3 losses** overall. All games ended under chess rules; none was counted as a draw merely because of a ply limit.

The first set was used during development; the final confirmation set was not used to choose another revision. These small, time-dependent samples do not establish an Elo rating or statistically reliable strength gain. The complete search-only ablation also remains useful for larger future comparisons; this pass does not claim the added evaluation terms outperform search-only with statistical confidence.

- [Eight final development games](benchmarks/2026-09-21-initiative-match.pgn) and [results](benchmarks/2026-09-21-initiative-match.json).
- [Final independent-opening games](benchmarks/2026-09-21-initiative-heldout.pgn) and [results](benchmarks/2026-09-21-initiative-heldout.json).
- [Initial candidate search measurements](benchmarks/2026-09-21-initiative-initial-search.json), [first eight results](benchmarks/2026-09-21-initiative-initial-match.json), [second eight results](benchmarks/2026-09-21-initiative-initial-heldout.json), and [ablation/reference probes](benchmarks/2026-09-21-initiative-experiments.json). The less successful trials are retained rather than replaced by the final result.
- [Historical Opera Game recheck](benchmarks/2026-09-21-initiative-opera.json), covering every White decision. Alternative sound opening choices are not test failures.

The final historical sample matches 14 of 17 White moves and retains `Nxb5`, `Rxd7`, `Bxd7+`, `Qb8+` and `Rd8#`. The purpose is to retain the calculated combinations, not to force the opening to reproduce one game.

## Search cost

Both binaries were compiled with the same installed Apple Command Line Tools clang 17. The benchmark ran each engine separately, with fresh game state for every position, one thread, 16 MB hash, Morphy style, depth six, ten positions and three repetitions. No other engine validation job ran concurrently.

| Measurement across 30 searches | Baseline dd001bd | Revised engine |
| --- | ---: | ---: |
| Total elapsed | 2.022 seconds | 2.535 seconds |
| Nodes | 1,530,051 | 1,733,664 |
| Aggregate nodes/second | 756,741 | 683,795 |

The sample takes **25.4% more time** and searches **13.3% more nodes**. The extra tactical coverage has a real cost; this is not a claim of faster search. Shared attack information avoids separate mobility/king/threat ray generation, and quiet-check work is deliberately bounded, but per-node throughput is still lower. Fixed-depth timings also reflect changed trees. Equal-time games are the relevant complementary check. See the [raw timing report](benchmarks/2026-09-21-initiative-search.json).

## Validation and packaging

The C++ Release suite passes **495/495**, including thirteen new tests for pins, king flights, sparse mating coordination, unconstrained endgame king activity, color reflection, special-move check prediction, board restoration, quiet mate and the shorter quiet preparation. The legacy assertions were retained.

Additional final checks pass: 346 Rust unit/integration tests, 16 documentation examples, 21 UCI process tests, an independent Stockfish oracle over 416 positions / 4,586 PV moves / 411 ponder replies, and 94 selected C++ UBSan tests. See the [current baseline](current-baseline.md). AddressSanitizer could not start under the installed CLT runtime: even a minimal standalone `puts` program hung before `main`. The engine ASan executable also timed out outside the sandbox. This is not an ASan pass. Windows execution and fresh Linux validation remain outside this pass; prior platform results remain historical evidence.

The final macOS ARM64 UCI binary has SHA-256 `bd00e3b1c4b24126c1468e04ce825d3fe9b00746038eab9db4e8bc88a127a0e2`. Its source is `dd001bd037121440dc5f0c7714ed8d0881c8b633` plus this working-tree change. The installer now marks uncommitted checkouts with `-dirty`, alongside the binary hash, so exported games do not falsely attribute the build to an unchanged commit. No app interface wording is changed.

That exact binary is installed in the engine's release directory, the Unity project's StreamingAssets and the existing Mac app. The app passed strict/deep signature verification after re-signing, and its packaged engine passed a real UCI `a3` mate-in-two smoke check. The prior engine, manifests and app signature files were backed up. Start a new game to use this version. The new JSON artifacts parse, and all 80 saved trial/final PGNs replay legally over 8,615 plies.

## Reproduction and next steps

With the dependencies from `scripts/requirements-uci.txt` installed and the previous binary saved separately:

```sh
python scripts/benchmark_core.py rust/target/release/opera-uci --morphy --depth 6 --repeats 3 --output /tmp/initiative-search.json
python scripts/match_core.py rust/target/release/opera-uci /path/to/baseline --morphy --baseline-morphy --baseline-name dd001bd --movetime 0.25 --openings-file docs/benchmarks/2026-09-21-initiative-heldout-openings.json --output /tmp/initiative-match
python scripts/match_core.py rust/target/release/opera-uci /path/to/baseline --morphy --baseline-morphy --baseline-name dd001bd --movetime 0.25 --openings-file docs/benchmarks/2026-09-21-initiative-confirmation-openings.json --output /tmp/initiative-heldout
python scripts/audit_style.py docs/games/morphy-opera-1858.pgn --engine rust/target/release/opera-uci --stockfish /path/to/stockfish --output /tmp/initiative-opera.json
```

Time-limited choices and game results can vary with hardware and load. Fixed nodes/depth and proven mates are useful complementary regressions. The local default Xcode compiler currently requires license acceptance; this pass uses the separately installed Command Line Tools without changing the license or global developer-directory setting.

## Findings retained for the next development session

The walkthrough with the owner reaffirmed purposeful, coordinated attacking play as the direction: develop the whole army, create threats that demand a response, preserve the pieces that restrict the king, and calculate the finish. The implemented changes improve both positional estimates and tactical coverage; neither alone establishes that a sacrifice is sound. Quiet-check extensions mean noncapturing checks at the horizon, not an extension of every quiet preparatory move. The quiet `a3!` result is a calculated mating proof, not a special-case rule or historical move lookup.

Carry these priorities into the next core pass:

1. **Exposed-king defense first.** Reproduce the 2700-target game's moves 29–30 with the saved history. The lower positive scores are still wrong, and the new `Rad1` choice is not a defensive improvement. Distinguish the static evaluation error from missed defensive continuations before changing attack weights.
2. **Preserve sound initiative.** Retain the quiet mate, `Nb5+`, useful defender/pin behavior, endgame king activity and the Opera Game combinations. Broaden quiet-tactic, defensive-resource and refuted-sacrifice coverage; stronger rook activation at move 19 remains an opportunity.
3. **Measure the speed tradeoff.** Profile the shared attack evaluation and bounded check search. The current fixed-depth sample costs 25.4% more time. Compare at equal move time as well as fixed depth/nodes, including the retained search-only candidate, before claiming a strength or speed gain.
4. **Use fresh evidence.** Run longer paired matches on independent openings and review new exported games. The 5/8/3 result is a small development/confirmation sample, not an Elo measurement. Rejected attack-heavy candidates remain useful negative examples.

Opponent adaptation remains a separate future root-selection experiment with an explicit UCI toggle, initially off. It must preserve the best-defense evaluation and forced wins, keep opponent history out of position-value/transposition scores, and reset/deduplicate learning across new games and review/resume. Mistaking a losing position for a winning one is a core defect, not intentional practical risk. The neural value network remains distinct from opponent move prediction; neither feature is implemented in this pass.
