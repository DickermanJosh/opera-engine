# Human–Opera, 20 September 2026: game review and priorities before neural evaluation

Opera won as Black in 24 moves, ending with **24...Be2#**. The game demonstrates the requested character: a pawn invested in development, early castling, participation by all four minor pieces, pressure on open lines, a decisive quiet combination, and a coordinated queen-and-bishop finish.

The next tuning goal should be **better judgment about building and completing coordinated mating attacks**. Development, activity and material investment should serve that objective. The current engine already has substantial development and sacrifice incentives. This game also contains useful examples of activity that stronger defense could neutralize.

## Style objective: judge pieces by their role in the mating attack

Piece retention should depend on the position and the intended combination. A piece may be essential because it delivers mate, controls a flight square, supports an entry square, pins a defender, or prevents a counterattack. A sacrifice or exchange may be essential because it opens a line, removes a defender, draws the king onto a vulnerable square, or clears a route for another attacker. These roles apply to every piece type; there is no general instruction to preserve bishops or the bishop pair.

In this game the knight on f6 could be allowed to fall while the queen invaded on e1, and the remaining bishop later delivered mate on e2. The analysis of `9...Bxc3` below is a position-specific comparison against best defense. It does not establish a general preference for keeping bishops, nor does the eventual mating finish prove that every earlier exchange was necessary for that finish.

Use mating patterns and nets to guide evaluation and direct search toward promising combinations. Assess legal king escapes, safe checking squares, cooperation between attackers, and the defender's resources. A mating-net heuristic must remain a bounded positional estimate; reserve mate scores for a forced continuation established by search. Include quiet preparation and non-checking sacrifices, because the move that creates a decisive threat may not itself give check. When a direct attack is unavailable, development and prevention of counterplay should prepare the next opportunity; retain the ability to convert a material advantage if the opponent avoids mate by conceding material.

## Game and verification

- Source: `/Users/josh/Documents/Opera Chess/Games/Opera-20260920-004245-447-bee1.pgn`.
- Engine commit: `47f0442eeb530912ab8de6f9c8a3d9a26cccf409`.
- Binary SHA-256: `a5e0b8932063d36d4af2ff0b8b1b9da4babbb1772f50f2757401bfe1c1fa8a33`.
- Exported settings: `MorphyStyle=true`, `MoveTimeMilliseconds=1000`. The export records the setting at export, not individual move times.
- All 48 plies are legal; the final position is checkmate and exactly matches the exported `FinalFEN`.
- Independent analysis: local **Stockfish 17.1**, one thread, 64 MB hash, full game history, hash cleared before each position. Initial screening used 250,000 nodes per position; eleven key positions received 2–6 million nodes, with up to three principal variations. Two bound-valued results were checked again using the last completed, exact-scored iteration from a five-million-node search.
- Scores below are Stockfish scores from **White's perspective**. They are bounded-search estimates, not proofs of the opening's theoretical value. The original PGN's evaluation comments are Opera's own assessments and are a separate source.

```text
1. e4 d5 2. exd5 e6 3. dxe6 Bxe6 4. d4 Nf6
5. Nf3 Bb4+ 6. Nc3 O-O 7. Bd3 c5 8. O-O Nc6
9. dxc5 Bxc3 10. bxc3 Qa5 11. Bd2 Qxc5 12. Qe2 Rfe8
13. Nd4 Bg4 14. Qxe8+ Rxe8 15. Rfe1 Rxe1+ 16. Rxe1 Nxd4
17. cxd4 Qxd4 18. Bg5 Qb4 19. Bxf6 Qxe1+ 20. Bf1 Be2
21. h3 gxf6 22. Kh2 Bxf1 23. Kg3 Qe5+ 24. Kf3 Be2# 0-1
```

## What to preserve

**The opening has a coherent purpose.** After `2...e6 3.dxe6 Bxe6`, Black has sacrificed one net pawn: Black lost the d- and e-pawns, while White lost the e-pawn. Black gained a developed bishop and open central lines. Both bishops and the king's knight were developed before `6...O-O`; `8...Nc6` brought out the fourth minor piece. `7...c5` challenged White's remaining central pawn. Both rooks subsequently participated on the e-file.

**13...Bg4 is the centerpiece.** Moving the bishop away from e6 uncovers the e8-rook's attack on the queen at e2, while the bishop attacks that queen through f3. Stockfish's top three lines after `13.Nd4` scored `13...Bg4` at **−3.83**, versus approximately **−0.11** for `13...Bd5` and **−0.07** for `13...Nxd4`, all at depth 24. Opera found the decisive move at the app's short thinking time. In the game White gave the queen for a rook; alternative defenses include giving other material, so this should not be described as an unavoidable queen capture.

**18...Qb4 maintains the win while creating another threat.** It attacks the rook on e1 along b4–c3–d2–e1, allowing the knight on f6 to be taken. Stockfish ranked it first at **−4.67**, essentially level with the quieter `18...Kh8` at **−4.66**. The game's `19.Bxf6 Qxe1+` illustrates a tactically justified preference for activity while a piece is attacked.

**The finish is concrete.** Deeper Stockfish analysis confirms a forced mating continuation beginning with `21...gxf6`, and prefers `22...Bxf1` to taking that bishop with the queen. Opera then found `23...Qe5+` and the immediate mate after `24.Kf3`. This is useful evidence of conversion and mating ability in this position; the game does not test endgame technique.

## Where stronger judgment would help

| Position or decision | Independent analysis | Implication |
| --- | --- | --- |
| After `3...Bxe6` | About **+1.10**, completed depth 28 | Black has practical activity, but this search still prefers White. The eventual win does not establish full compensation for the gambit. |
| `5...Bb4+` | After it, **+1.64**, depth 28, with `6.c3`. Before it, MultiPV preferred `5...c5` at **+1.09**, depth 24. | A developing check can invite a useful pawn tempo. Judge the opponent's best response, including declining a pin. |
| After `9.dxc5` | `9...Bxc5` approximately **+1.02**, depth 25. After the played `9...Bxc3`, **+1.94**, depth 27. | Keeping an active bishop and recovering the pawn was stronger here than exchanging it to double White's pawns. |
| After `12...Rfe8` | **+0.68**, completed depth 26, with `13.Qe3` | White could still meet the pressure. `13.Nd4` allowed the decisive combination. |

These comparisons use separately searched positions and sometimes different depths; differences are approximate. They identify positions worth testing, rather than precise centipawn losses for a scorecard.

Fresh one-second replays using the exact exported Opera binary selected `2...e6` with Morphy style enabled and `2...Nf6` with it disabled. Both modes selected `9...Bxc3` and found `13...Bg4`. Thus the gambit preference responds to the style setting, while the bishop-exchange issue also involves the base evaluation/search. These are individual replays, not strength or speed benchmarks.

## Recommended bounded pass before the NN

1. **Recognize each piece's role in a mating attack.** Refine the value of open lines, supported entry squares, restricted king escapes, pins and removing defenders. Prefer retaining an attacker when its contribution is needed to build or complete the net; prefer exchanging or sacrificing it when that enables the decisive combination. Existing king safety already counts attackers around the king, and mobility excludes enemy pawn attacks; extend their understanding of cooperation and defensive resources. The contrast between `8...Bxc3` with a potential `...c4` follow-up and `9...Bxc3` after that pawn has disappeared is a useful contextual example to investigate.
2. **Calibrate attacking compensation against best defense.** Tie material investment to concrete gains such as opening a line to the king, eliminating a key defender or creating a sustainable mating threat. The current sacrifice allowance uses initiative, king safety and development, is capped at 100 centipawns, and disappears for material deficits beyond 400 centipawns. Audit overlapping rewards and abrupt thresholds. Keep the willingness to invest material while improving the estimate of what that investment achieves, including whether the defender can neutralize it.
3. **Improve the search for quiet forcing ideas and useful work per second.** Prioritize finding and verifying quiet mating threats, clearance moves and sacrifices that initiate combinations. The main search already extends checks and protects checks from reductions. Its quiescence search includes captures, promotions and check evasions, but no quiet checks when the side is not in check. Experiment with tightly bounded tactical coverage, improved quiet-move ordering and reduction decisions, using a varied tactical suite. Profile before changing the full move sort or synchronization in the single-threaded move-ordering path. Accept changes on both equal-time play and tactical reliability; higher nodes per second alone is insufficient.
4. **Establish an explicit style acceptance set.** Retain this game's quiet combination, tactical counterattack and mating finish. Add paired examples where preserving an attacker is essential and where sacrificing the same type of piece enables mate, plus contrasting positions where apparent nets fail to a defensive resource or require quiet preparation. Measure whole-army participation, productive pawn breaks, coordinated threats, sound sacrifices and forced-mate solutions alongside paired-game results. Hold some positions and openings out of tuning, test both colors, and use several time controls. Do not require this exact opening or count sacrifices/checks as inherently good play.

This is enough scope for one focused pass. Broader games and tactical/endgame checks can expose additional defects; there is no need to exhaust handcrafted evaluation improvements before beginning the NN.

## Carrying the style into the NN

My recommendation is to start with a compact CPU-oriented neural evaluator and retain a separately measurable, bounded style adjustment during the transition. Train and validate the base evaluator for positional accuracy, then compare the combined system against the present engine for both playing quality and the desired preferences. Calibrate the adjustment to avoid counting the same activity twice. Architecture size must be measured against search speed; the [Stockfish NNUE documentation](https://official-stockfish.github.io/docs/nnue-pytorch-wiki/docs/nnue.html#consideration-of-networks-size-and-cost) explains that accuracy/performance tradeoff.

Curated attacking positions should contribute to evaluation and validation, alongside positions where the apparent attack is unsound. A won game's opening positions are not automatically winning training labels. Position evaluations and game outcomes can be combined deliberately in training, as described in the [NNUE training documentation](https://official-stockfish.github.io/docs/nnue-pytorch-wiki/docs/nnue.html#using-results-along-the-evaluation). The proposed style adjustment is a design recommendation for Opera, not a claim that this is Stockfish's approach.

This review changes no engine or application behavior. The original saved game is unchanged.
