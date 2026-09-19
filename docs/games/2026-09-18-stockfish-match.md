# Opera vs Stockfish — September 18, 2026

Opponent: Stockfish 17.1, full strength. Opera uses its default handcrafted evaluator (`MorphyStyle=false`).

Both engines use one thread, 16 MB hash, no pondering, and equal clocks of 30 seconds plus 0.25 seconds per move. Both games start from the normal starting position. Played locally on macOS ARM64. These two quick games are a development snapshot, not an Elo estimate.

| Game | Opera color | Result (White–Black) | Ending | Plies |
| --- | --- | --- | --- | --- |
| 1 | White | 0-1 | checkmate | 52 |
| 2 | Black | 1-0 | checkmate | 55 |

## Game 1 — Opera as White

```text
1. Nc3 d5 2. e3 e5 3. Qh5 Nc6 4. Bb5 Nf6 5. Bxc6+ bxc6 6. Qxe5+ Be7 7. Nf3 O-O
8. O-O Bd6 9. Qg5 Re8 10. Rb1 h6 11. Qh4 Bg4 12. Nd4 c5 13. Ndb5 g5 14. Qxh6
Nh7 15. Nxd6 cxd6 16. f3 Re6 17. Qxh7+ Kxh7 18. fxg4 d4 19. exd4 cxd4 20. Nd5
Qa5 21. Rxf7+ Kg6 22. Rf1 Qxd5 23. Ra1 Re2 24. Rf2 d3 25. Rxe2 dxe2 26. a3
e1=Q# 0-1
```

## Game 2 — Opera as Black

```text
1. e4 Nc6 2. d4 Nf6 3. d5 Ne5 4. f4 Ng6 5. e5 Ng8 6. Bd3 e6 7. Qe2 exd5 8. f5
N6e7 9. Nf3 c5 10. c4 dxc4 11. Bxc4 Nxf5 12. O-O Ngh6 13. g4 Nxg4 14. Ng5 Ngh6
15. Rxf5 Nxf5 16. Bxf7+ Ke7 17. Nc3 Qa5 18. Bd2 Kd8 19. Bc4 Nh6 20. e6 Qb6 21.
Nd5 Qd6 22. Bf4 dxe6 23. Bxd6 exd5 24. Qe5 Bd7 25. Ne6+ Bxe6 26. Qxe6 d4 27.
Bb5 Bxd6 28. Qd7# 1-0
```

The PGN includes per-move clock annotations. All moves were checked by python-chess; the client reported no protocol warnings.
