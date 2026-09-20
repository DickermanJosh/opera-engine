# Development emphasis and game review — 2026-09-20

This pass responds to knight-heavy openings and the request to inspect and share games directly from the Unity app. Engine changes are on `engine-core`; the app changes are on `opera-integration` in the sibling `chess` repository. Neural evaluation is still deferred.

## Development and activity

The old extra Morphy development weight was only 0.2 times a ten-centipawn minor-piece bonus. Centralization and mobility could easily make a second knight move more attractive than involving the rest of the army. Simply increasing that bonus still favoured knights, which can leave home without a preparatory pawn move.

The new opening terms, at the default Morphy bias, are:

- 44 cp for each minor off its back rank, plus up to 16 cp for useful mobility. Each piece's mobility contribution saturates; one mobile knight cannot accumulate the participation credit of several pieces.
- 12 cp extra for each developed bishop; 48 cp cost for each home bishop without a safe exit. Opening a diagonal earns up to 18 cp of readiness before the bishop moves.
- 20 cp for a central pawn on the third rank; 50 cp for a pawn occupying d4/e4/d5/e5. Advancing from e4 to e5 retains that central foothold.
- Minor-piece central occupancy/control, a small bonus for multiple developed pieces, and 18 cp for connected rooks.
- A cost for pawn-attacked minors and for an early knight that can be attacked by a pawn push while other minors remain at home. Premature queen excursions also cost development time.

These are position-based terms, so transpositions do not depend on an unrecorded “piece moved twice” counter. They fade linearly from full material to half the opening phase and disappear thereafter. Bias zero still exactly matches the normal handcrafted evaluator. The shared mobility pass collects the needed attack information once; Morphy no longer generates the same bishop/rook rays again for its new terms.

This is a strong stylistic preference, not an opening book or a prohibition on knight moves. Search can still choose forcing tactics and sacrifices. Regressions retain the Opera Game's `Qb8+!` mate, an undefended queen capture, and castling over an unnecessary king walk. New regressions cover bishop readiness, distinct-piece participation, central occupation, pawn kicks, colour symmetry, history-independent scoring, zero bias, and opening lines for bishops after `1.e4 Nc6 2.d4`.

## Evidence

The reference binary is saved `352954a`, SHA-256 `c45b7a494f7dffe21ae0590595f5cab325c1c873e9f49dc10cd5c9ecdd0fbaa3`. Both sides use Morphy style for this comparison. The updated match harness records both style settings and accepts the baseline name instead of hard-coding an older commit.

Final macOS ARM64 evidence for binary SHA-256 `a5e0b8932063d36d4af2ff0b8b1b9da4babbb1772f50f2757401bfe1c1fa8a33`:

| Check | Result |
| --- | --- |
| Full C++ Release suite | 482/482 pass, including eight new development/tactical regressions |
| Native UCI process acceptance | 21/21 pass |
| Independent Stockfish legality oracle | 416 positions, 4,551 PV moves, 408 ponder replies |
| Docker Linux ARM64 | 21 process tests; 176 oracle positions, 1,909 PV moves, 171 ponder replies; external client acceptance passes with two completed drawn games |
| Eight paired games against Morphy build 352954a | 4 wins, 3 draws, 1 loss; all adjudicated, no ply-limit results |
| Fixed-depth sample | Total elapsed time +9.8%, searched nodes +6.2%; median time ratios by position range from 0.94× to 2.99× |

The heavier evaluation changes which branches the engine explores. Opening trees can take substantially longer at a fixed depth even though both versions honour the same move-time limit. Reusing the existing mobility attack lookups limits repeated work; the current C++ microbenchmark remains below one microsecond/evaluation. These observations are not a claim of universally faster search or measured Elo improvement.

In the final king-pawn sample, Black chose `1.e4 d5 2.exd5 e6 3.dxe6 Bxe6`: a one-pawn gambit followed by minor-piece development, castling on move 7, `...Re8` on move 8 and `...Nbd7` on move 9. The London sample developed both bishops and both knights by move 7 and castled on move 6. The complete, deliberately unfinished lines are retained for inspection; these examples do not establish that every sacrifice is sound.

Raw artifacts:

- [Fixed-depth search sample](benchmarks/2026-09-20-development-search.json): ten positions, three repetitions, depth six, one thread and 16 MB hash.
- [Paired match results](benchmarks/2026-09-20-development-match.json) and [all eight games](games/2026-09-20-development-match.pgn): 100 ms/move, four starting openings with colours reversed. No Elo estimate follows from eight short games.
- [Opening samples](benchmarks/2026-09-20-development-openings.json) and [PGNs](games/2026-09-20-development-openings.pgn): Black at one second/move against Stockfish 17.1 at 4,000 nodes/move, stopping after move 12. These games are deliberately unfinished. The London sample fixes `1.d4 Nf6 2.Bf4`; Black's later development is the observation. Captures can reduce the reported count of developed pieces.

Reproduce with the saved baseline kept separately from the candidate:

```sh
python3 scripts/benchmark_core.py rust/target/release/opera-uci --morphy --depth 6 --repeats 3 --output /tmp/development-search.json
python3 scripts/match_core.py rust/target/release/opera-uci /path/to/saved/opera-uci --morphy --baseline-morphy --baseline-name 352954a --movetime 0.1 --output /tmp/development-match
python3 scripts/sample_openings.py rust/target/release/opera-uci /path/to/stockfish --output /tmp/development-openings
```

These helpers require `scripts/requirements-uci.txt`. Timing and chosen moves vary with hardware and load. Fixed-depth time includes changes in the searched tree and is not a pure evaluator-speed benchmark. Native Windows remains unverified; the Unity app has only been executed on macOS ARM64.

## Review, export and analysis

The app now keeps the live game separate from the displayed historical position. Clicking a SAN move, using Start/Back/Next/Live or pressing Left/Right/Home/End navigates the complete scrollable transcript. A live engine reply may finish while reviewing; it updates the transcript without moving the viewed board. Resume saves the original PGN before replacing the continuation and reconstructs the selected state with castling, en passant, clocks and repetition history intact.

Export saves a normal PGN in the documents game folder and copies it to the clipboard. It includes result/termination, engine revision and binary hash, the current thinking-time setting, viewed position, final FEN and White-relative evaluation annotations when available. Show exports opens that folder. The original line is also archived before a new game, resume or return to menu; closing the app attempts to save a nonempty game.

A separate bounded analysis process evaluates the displayed position on either turn, with the playing process also exposing its search updates. Hiding analysis or changing position cancels that work. Generation checks reject stale callbacks. Three candidates are obtained with successively excluded root moves, at 150/500/1,500 ms per candidate in each refinement pass. Their individual depths are visible; they are not exact equal-depth MultiPV. The main meter shows the unrestricted search's estimate and uses a visual scale, not a calibrated winning probability. Scores remain White-relative when the board is flipped.

Unity validation adds 603 independent SAN fixtures to the existing 494 legal-move/FEN fixtures, both perft checks and adjudication checks. History tests cover non-mutating review, branching, castling, en passant, repeated positions after resume, resignation and mate PGN results. Client checks cover streamed scores/PVs, root exclusions, cancellation, restart and process cleanup. Independent python-chess parsing confirms exported games reconstruct the exact final FEN.

Desktop checks exposed and fixed a stale renderer reference when returning from a historical board. Normal live moves now reuse their renderers; review changes rebuild the bindings safely. Visible checks include reviewing during search, resuming with an automatic archive, clicking transcript moves, keyboard navigation, hiding/showing analysis, candidate scores on both turns and PGN export/folder opening. See [user controls and packaging](unity-integration.md).
