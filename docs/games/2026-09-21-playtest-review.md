# Opera playtest review — 21 September 2026

The new exports show the intended attacking character in several different forms: queen-and-bishop pressure, a queen-and-knight double check, a pawn mate, and a rook-and-knight finish after exchanging queens. One specific improvement opportunity is recognizing a quiet move that closes a king's escape square and shortens a forced mate.

## App rebuild

Rebuilt the pulled Unity `main` commit **c09f532** (`style update`) with Unity 6000.0.54f1 using `OperaIntegrationTools.ValidateAndBuildMac`. Rules/perft, 603 SAN fixtures, review/resume/export checks, 15 gesture cases, highlight state, and the real UCI client checks passed. The rebuilt macOS ARM64 app was opened and the updated menu inspected; the startup player log contained no exception/error lines.

App: `/Users/josh/dev/Actice Projects/chess/build/Opera Desktop/Opera Chess.app`.

The packaged engine remains **47f0442**, SHA-256 `a5e0b8932063d36d4af2ff0b8b1b9da4babbb1772f50f2757401bfe1c1fa8a33`, matching the reviewed games. No engine or app source was changed for this review. Unity's two automatic project-settings edits were restored to the pre-build contents.

## Saved-game inventory

The directory `/Users/josh/Documents/Opera Chess/Games` contained **14 exports of at least eight plies**, representing **eight distinct move sequences** after removing duplicate exports. Six are completed Opera wins by checkmate, including the game reviewed previously; two are unfinished. Thus there are **five newly reviewed completed wins**, four with Black and one with White. All are labelled Human versus Opera, with the exported thinking-time setting at 1,000 ms.

Short development smoke-test exports were excluded. Duplicate exports and reviewed positions inside an existing game were not counted as additional games. All recorded moves are legal and final positions agree with the exported FENs after normalizing en passant notation.

| First export on 20 September | Opera | Result | Notable continuation |
| --- | --- | --- | --- |
| 00:42:45 | Black | Win, 24 moves | Previously reviewed: `13...Bg4`, ending `24...Be2#`. The 14:45 export is the same game. |
| 14:53:00 | Black | Win, 5 moves | Punishes an exposed king with `4...Qxh4+ 5.g3 Qxg3#`. |
| 15:02:00 | Black | Win, 28 moves | `24...Bxf4 25.exf4 Qb6+`, followed by a queen-and-knight mating sequence. |
| 15:04:26 | White | Win, 17 moves | Knight fork, king pursuit, and `17.b4#`; a faster quiet mating route existed. |
| 15:10:31 | Black | Win, 21 moves | Against the London setup: `19...Qg5 20.Nf3 Bxf3 21.Qd7 Qxg2#`. The 18:28 exports are duplicates. |
| 18:30:37 | Black | Unfinished, 7 moves | All four minor pieces developed and castled; independent analysis slightly prefers Black. |
| 18:34:02 | Black | Win, 25 moves | Exchanges queens on move 13; finishes with `24...Nf3+ 25.Kg2 Rg1#`. |
| 18:35:48 | Black | Unfinished, 11 moves | The accepted development gambit; independent analysis slightly prefers White. |

This is evidence about behavior and conversion against this human playtest sample. It does not establish an Elo rating or results against other engines.

## Combinations worth preserving

**The 28-move game: clearing a diagonal and using double check.** `24...Bxf4` offered the bishop to the e-pawn. After `25.exf4`, e3 was cleared for the queen's b6–g1 diagonal. The game continued `25...Qb6+ 26.Kh1 Nf2+ 27.Kg1 Nxh3+ 28.Kf1 Qf2#`. The capture on h3 gives double check: the knight checks from h3 while uncovering the queen's diagonal. Stockfish confirms mate in three after `26.Kh1`. Earlier, `26.Be3` could have prevented that particular mating sequence, although Black remained decisively ahead; the bishop offer should not be described as forcing mate against every defense from move 24.

**The London game: maintaining a mating threat while taking a defender.** `19...Qg5` coordinates with the bishop on c6 against g2. After `20.Nf3`, `20...Bxf3` is Stockfish's clear first choice: approximately −5.67 from White's perspective versus −1.92 for its next candidate, at completed depth 21. This connects a capture to the king attack. White could still prevent immediate mate with `21.g3`; the played `21.Qd7` allowed `21...Qxg2#`.

**The 25-move game: a mating attack without queens.** The queen exchange did not end Opera's attacking play. Its rooks and knight subsequently coordinated against the exposed king. After `24.h3`, `24...Nf3+ 25.Kg2 Rg1#` is forced. This fits the stated preference that exchanges be judged by what the remaining pieces can accomplish together.

## A concrete next improvement: quiet mate preparation

In the game where Opera played White, after `14...Kc5`, it chose:

```text
15.Qd5+ Kb4 16.Qd2+ Kc5 17.b4#
```

A faster route was **15.a3!**, a non-checking move that takes b4 away from the king. It forces mate on White's next move, for example:

```text
15.a3 dxe5 16.Qd5#
15.a3 Ne7 16.b4#
```

This was checked exhaustively with independent legal move generation: **all 16 legal Black replies after 15.a3 allow mate in one**. Stockfish also ranks it as mate in two, while `15.Qd5+` and `15.b4+` require three moves against best defense. Opera still converted its chosen line correctly; the refinement is finding the shorter combination whose first move is quiet.

Reproduction position, White to move:

```text
N1b2bnr/1p4pp/p2p4/2k1Np2/2P5/8/PP3PPP/R2QKB1R w KQ - 2 15
```

This is a useful candidate for the future mating-pattern/search regression set: the supporting pawn's control of an escape square matters more than giving an immediate check. The review does not diagnose which search heuristic caused the choice or change the engine to require a particular move.

## Analysis method and limits

Used local Stockfish 17.1, one thread, 64 MB hash, full game histories, and a cleared hash for each position. Screened **234 positions** across the seven newly reviewed distinct games at 150,000 nodes each. Nine selected positions received 0.5–3 million nodes, with up to three candidate lines. Reported evaluations use completed iterations without upper/lower-bound flags; MultiPV comparisons use a common completed depth. Principal variations were checked for legal moves.

The two unfinished final positions evaluate at approximately **−0.41** (18:30 game, depth 20) and **+0.57** (18:35 game, depth 26), with positive scores favoring White. Neither is a recorded win. These are search estimates, not exact game-theoretic values.

Scratch evidence: `/tmp/opera-games-review-20260921-inventory.json`, `/tmp/opera-games-review-20260921-screen.json`, `/tmp/opera-games-review-20260921-deep.json`. Build log: `/tmp/opera-20260921-unity-build.log`. The previous game's more detailed analysis is in [the 20 September review](2026-09-20-user-game-review.md).
