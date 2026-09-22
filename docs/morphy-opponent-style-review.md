**Morphy, practical risk, and the Opera Game — 2026-09-21**

This is the pre-change audit. The subsequent [attacking-principles implementation and measurements](attacking-principles.md) record the source changes and their validation; the observations below describe the earlier packaged engine.

The current attacking identity is working. At three seconds per decision, Opera with Morphy style chose **14 of the 17 historical White moves**, including every move from 5.Qxf3 through 17.Rd8#. The three alternatives were sound opening choices, not tactical failures. The standard evaluator matched 11 moves and, in this sample, handled the central attacking decisions less convincingly. This is a diagnostic on one famous game, not a strength estimate or a reason to train the engine to memorize its moves.

The next priority is more accurate judgment of **whose attack is real, which defenders matter, and when to bring the remaining pieces into play**. Opponent-aware risk is a separate decision policy. It cannot repair an engine that already believes a losing position is winning.

This pass adds an annotated historical PGN, analysis of five recent bot games, saved reference searches, and a reusable offline audit tool. It does not change the playing evaluator, search, app, or introduce a working opponent model. The observations below distinguish measured behavior, source-code findings, and proposed changes.

**What the recent games establish**

Five distinct completed user bot games were available, after excluding repeated exports and the earlier development smoke tests:

| Opera side | Stockfish UCI target | Time per move, each side | Result for Opera | Full moves |
| --- | ---: | ---: | --- | ---: |
| White | 1800 | 3 seconds | Win | 43 |
| White | 2103 | 3 seconds | Win | 51 |
| White | 1618 | 3 seconds | Win | 27 |
| Black | 1800 | 1 second | Win | 35 |
| White | 2700 | 3 seconds | Loss | 64 |

All 437 played half-moves were legal, final FENs matched the exports, and all five recorded results agreed with checkmate on the final board. The high-strength loss is tagged **2700**, rather than 2800, in its export. The results are encouraging but do not establish Opera's Elo or a modern numerical equivalent for Morphy. Stockfish's strength controls deliberately select weaker moves; performance against that policy should not be treated as identical to human move selection at the same nominal number. [Stockfish's official explanation](https://official-stockfish.github.io/docs/stockfish-wiki/Stockfish-FAQ.html#how-do-skill-level-and-uci_elo-work)

The app's PGN evaluations come from whichever bot was about to move. Consequently successive annotations alternate between Opera and Stockfish, even though both use White's perspective. Comparing them as one continuous evaluation series would be misleading. The audit instead used a separate, full-strength Stockfish 17.1 process for every position.

Three useful cases emerged from deeper checks:

| Position | What happened | Independent evidence | Development implication |
| --- | --- | --- | --- |
| 2700 game, **19.bxa6** | Opera collected another pawn while Black was preparing activity against its king. It reported about +3.98 at depth 10. | An 8-million-node, three-line reference search preferred **19.Rfd1** (+2.34) and **19.Rad1** (+2.23). A separate common-depth candidate comparison still favored both rook moves, while **bxa6** evaluated at 0.00. | Activate a rook and contest the d-file before extending a pawn collection. This is the same broad principle as Morphy's 14.Rd1. |
| 2700 game, **29.Qh8+ / 30.Ne2** | Opera continued to report about +3.80 as the opponent's heavy pieces invaded. Fresh three-second searches reproduced both choices. | The reference preferred **29.fxg5** to the check. By move 30, a 24-million-node restricted confirmation judged all three tested continuations losing. | Recognize the attacking side's threats and the urgency of defense. The earlier favorable assessment was unreliable; this was not an explicit, informed gamble on a weak opponent. |
| 2103 game, **20.Qxf6** | Opera exchanged queens and later won after further errors. | The reference found **20.Nb5+** at about +3.58; exchanging queens was roughly equal (−0.15 in the unrestricted comparison, −0.33 in the candidate comparison). Both Opera evaluators chose the exchange at three seconds; Morphy still did at ten seconds. | Improve recognition of a sustainable attack and the value of retaining its pieces. More caution alone would not fix this missed opportunity. |

These reference scores are finite-search estimates, not proofs, and their numerical scale is not calibrated to Opera's evaluation. The initial 200,000-node screening was only a way to select positions. It gave unstable rankings at move 30; even the longer searches changed their defensive ordering. In particular, an intermediate search reported mate after Ne2, whereas the 24-million-node confirmation returned a non-mate losing score. Do not turn that intermediate mate score, or a particular preferred defense there, into a golden test. The robust finding is the large mismatch in assessment and the earlier failure to use the rooks.

The problem persists with Morphy style disabled: at 19 the standard evaluator also chose bxa6 and reported +3.97; at 30 it chose Ne2 and reported +3.15. Reducing the style multiplier is therefore not an adequate diagnosis.

**Every White decision in the Opera Game**

The historical score is Morphy versus the Duke of Brunswick and Count Isouard, Paris, 1858. The precise day and opera performed are disputed, so the PGN leaves the month/day unspecified. The source establishes the score; the interpretation and engine comparisons below are this audit's own analysis. [Historical score and source discussion](https://en.wikipedia.org/wiki/Opera_Game)

| Move | Principle visible in the position | Current code and observed choice |
| --- | --- | --- |
| **1.e4** | Occupy the center while freeing the queen and king's bishop. Pawn moves serve piece access. | Central footholds and bishop exits are explicitly rewarded. Opera chose **d4**, another sound way to begin. |
| **2.Nf3** | Develop a piece with a concrete threat against e5, making progress while demanding a response. | Development and mobility exist; tempo through specific threats is mainly discovered by search. Opera chose **Nf3**. |
| **3.d4** | Challenge the center and prepare open lines for the rest of the army. | Central occupation is explicit. The value of opening the position at the right moment is more indirect. Opera chose **Nc3**, a sound alternative. |
| **4.dxe5** | Resolve central tension tactically. A relative pin is not a reason to stop calculating. | Legal move generation and search handle the combination; there is no explicit assessment of how a pin restricts useful defense. Opera chose **Nc3**, also strong in the reference search. |
| **5.Qxf3** | Recapture with intact kingside pawns and place the queen where it can work with Bc4. An early queen move can have a concrete job. | The early queen penalty is a soft preference, so search can override it. Opera chose **Qxf3**. |
| **6.Bc4** | Develop toward f7 so multiple pieces can cooperate against the same target. | Development and king-zone attacks partially capture this. Target-specific cooperation is not explicitly represented. Opera chose **Bc4**. |
| **7.Qb3** | Create two problems, f7 and b7. The defensive Qe7 obstructs the f8 bishop and gives the queen a binding responsibility. | Search sees threats, but defender workload, overload, and denial of the opponent's development have no dedicated terms. Opera chose **Qb3**. |
| **8.Nc3** | Add another participant while retaining the initiative instead of collecting b7 immediately. | Whole-army development is strongly represented. Opera chose **Nc3**. Qxb7 is also objectively good; this is not a rule that taking material is wrong. |
| **9.Bg5** | Develop the last surviving minor and pin Nf6 to Qe7, reducing defensive freedom. | Minor activity is explicit, but mobility uses pseudo-attacks rather than a role-aware defender map. Opera chose **Bg5**. The rooks are not yet developed. |
| **10.Nxb5** | Remove b5 and induce the c6 pawn to leave the diagonal toward e8. Sacrifice material to change access to the king. | Search finds it; there is no explicit line-opening or deflection feature. Opera chose **Nxb5**; standard chose the substantially weaker Bxf6 in this run. The sacrifice remains strong when Black declines it. |
| **11.Bxb5+** | Use the newly opened line with tempo and restrict the opponent's choices. | Checks receive search extensions. Opera chose **Bxb5+**. Nbd7 is the historical reply, not the only legal defense. |
| **12.O-O-O** | Secure the king and put a rook on the critical file in one move. The appropriate castling wing follows the position's needs. | Both wings receive safety credit and rooks receive file activity credit. Opera chose **O-O-O**. |
| **13.Rxd7** | Remove a blocker/defender while exploiting the pin of the f6 knight to the queen. | Search finds the exchange sacrifice; generic compensation alone cannot explain its tactical soundness. Opera chose **Rxd7**. Black need not answer with the game's Rxd7. |
| **14.Rd1** | Bring the last rook into the attack. A quiet move can renew a forcing threat. | Rook mobility and file bonuses help, but quiet tactical setup receives no special search protection. Opera chose **Rd1**. |
| **15.Bxd7+** | Exchange the bishop whose job is clearance and retain the bishop needed for the eventual mating pattern. | No explicit unique mating-role score exists; calculation achieves it here. Opera chose **Bxd7+**. Qxd7 defends better than the game's Nxd7, though White retains a winning position. |
| **16.Qb8+** | Deflect the knight from d8. The queen is worth giving up because the surviving pieces finish the job. | Opera chose **Qb8+** and saw mate. Independent legal enumeration confirms Nxb8 is the only reply. |
| **17.Rd8#** | Coordinate rook and bishop: Bg5 protects d8 and denies e7; the rook finishes on the back rank. | Search correctly recognizes mate and chose **Rd8#**. This is the strongest example of retaining the piece for its role rather than its nominal value. |

The sacrifices in this game are sound attacking decisions. Black had better replies than those played at several stages, so the entire historical sequence should not be described as forced from move 10. The final Qb8+ combination is forced. There is no need to assume a weak opponent to justify these moves.

**What is already emphasized, and what is missing**

The active evaluator already rewards distinct developed minor pieces, usable bishop exits, central footholds, connected rooks, and activity against the king. It penalizes early knights that can be chased while their companions remain at home. Simply increasing those weights again would risk paying several times for the same advantage. Development and mobility already feed the base evaluation, Morphy adjustments, initiative, and sacrifice compensation. [Morphy evaluator](../cpp/src/eval/morphy_eval.cpp#L25)

The most relevant omissions are:

1. **King escape and the feasibility of the attack.** The current king term counts pawn shelter, king position and weighted attacks into the neighboring squares. It has no explicit safe-check access, flight-square, or mating-net assessment. It is scaled by total remaining material, which can reduce danger after exchanges even when a small attacking group still has a mating net. A diagnostic at move 30 of the loss returned king-safety terms of −31 for White and −78 for Black, despite White being the side in serious danger. The static total was +5.46 with the standard evaluator and +6.18 with Morphy. These leaf values are not search results, but illustrate why the search needs better guidance. [King safety](../cpp/src/eval/handcrafted_eval.cpp#L433)

2. **Defenders and piece responsibilities.** Distinguish a piece that can actually intercept, capture or block from one that merely attacks a square geometrically. Track absolute/relative pins, overloaded defenders, and removal or deflection of the last useful defender. For our own pieces, identify access to checking squares and coverage of king exits. This should help preserve Bg5 in the Opera Game without a blanket rule against trading bishops or queens. Pin-aware mobility and king-flight legality need separate treatment: pinned pieces still attack squares for the rules governing king moves. [Mobility and activity collection](../cpp/src/eval/handcrafted_eval.cpp#L480)

3. **The remaining army and useful rook access.** A piece being off its home rank is not enough. A rook contesting the only invasion file can be worth more than a distant extra pawn. Existing open-file/connected-rook terms partly address this, but they do not explicitly measure a rook's role in preventing an invasion or extending a mating operation. The historical 14.Rd1 and the user's missed 19.Rfd1/Rad1 belong in the same evaluation corpus.

4. **Quiet tactical preparation and quiet defense.** At ordinary search nodes, quiet moves can be reduced or pruned. Checks get extensions; quiet mate preparation does not. At the capture-search horizon, noncapturing checks are absent when the side is not already in check, and negative-exchange captures are skipped unless they check or promote. That can hide a line-opening sacrifice or a preparatory move beyond the horizon. These are code-level risk factors, not a proven sole cause of the game mistakes. Test bounded checking continuations, threat-aware move ordering, and exemptions for critical quiet moves separately, while measuring the search cost. [Main search](../cpp/src/search/alphabeta.cpp#L116), [quiescence](../cpp/src/search/alphabeta.cpp#L175)

5. **One coherent compensation model.** The current extra sacrifice allowance uses initiative, king safety and development, caps the bonus at 100 centipawns, and disappears for material deficits greater than 400. This is an approximation; a valid rook or queen sacrifice must be justified by calculation or durable positional compensation, not by extending a flat material allowance. Audit the repeated use of activity features and replace discontinuities only with measured evidence. [Compensation](../cpp/src/eval/morphy_eval.cpp#L182)

These features must apply symmetrically. Detecting the opponent's mating net is as essential as constructing Opera's own. No generic bonus should override a demonstrated tactical refutation.

**How opponent-aware play should work**

The requested behavior is feasible, but it needs two distinct questions: **What survives the best defense?** and **What defense is this opponent likely to find?** Current alpha-beta search tries to find strong play for both sides. It does not yet model an opponent's strength, uncertainty, or probability of recognizing a particular defense.

The research distinction matters here. Opponent-model search can fail because its model of the opponent is wrong, or because its own evaluation overestimates a position that the opponent assesses correctly. The latter remains dangerous even with a good opponent model. That is closely aligned with the loss above. Probabilistic models address uncertainty, but add computation and need validation. [Donkers and van den Herik, *Opponent Models in Games*](https://www.ercim.eu/publication/Ercim_News/enw57/donkers.html)

An appropriate design for Opera is:

- **Keep a best-defense analysis for each serious root candidate.** Preserve objective scores and variations. A sound attack remains eligible against every opponent; there is no rating threshold at which the engine should stop playing its native style. Among comparably sound moves, favor coordinated attacks and difficult defensive tasks.
- **Classify the defensive task before considering speculation.** Is there one defense or several? Is it an obvious capture, a forcing check, an exchange of queens, or a quiet counterintuitive resource? Does it require a short sequence or sustained accuracy? Consider the size of the punishment if it is found, along with how many continuations still give adequate compensation. Raw counts of legal replies are not probabilities.
- **Estimate an opponent profile with uncertainty.** Use informative decisions from the current game and, if deliberately supported later, identified previous games. Forced moves and routine opening moves carry little evidence. A player may be good at forcing tactics and poor at quiet defense; a single guessed Elo loses that distinction. Clock pressure is another context, not a permanent skill change. With little evidence, assume competent defense and keep speculation small.
- **Use the model at move selection, not to make the opponent artificially foolish inside the tactical search.** A candidate's expected game score can be computed over predicted replies, but it needs a conservative allowance for unexamined responses and model uncertainty. Compare this against the best-defense baseline and enforce an explicit downside budget. When comfortably winning, protect the conversion; when worse, allow more complications if they improve practical chances. Never replace a found forced win with a merely hoped-for one.
- **Keep score reporting honest.** Display the best-defense evaluation on the normal meter. Record any separate practical assessment and why a risk was accepted. Do not label a refutable sacrifice objectively winning because the opponent is expected to miss its defense.

In mathematical terms, a prototype could blend best-defense value and expected value using confidence in the opponent model, with a separate maximum-regret constraint. The values should represent calibrated expected game points, not an arbitrary softmax over centipawns. A formula alone does not produce a validated defense probability; no numerical confidence thresholds or sacrifice budgets are claimed as tuned here.

There is an unavoidable tradeoff: intentionally choosing a move known to fail against correct defense accepts some objective risk in exchange for predicted practical chances. The model can make that decision more informed; it cannot make the refutation disappear. A modest soundness allowance initially, followed by measured expansion, is the appropriate path.

Implementation should leave position-only evaluation and transposition-table scores independent of opponent history. Put the profile and root decision policy outside them. Otherwise the same board can reuse a score calculated for a different opponent. Reset learning on a new game; deduplicate repeated UCI positions and analysis calls; do not learn from review navigation or treat a resumed alternative line as additional independent evidence. Stockfish target strength may serve as an optional prior in a bot match, but should not be relabeled as a human rating.

A future value network and an opponent policy are different models: one assesses positions, the other predicts choices. Human prediction needs human move data and held-out validation. Maia's research directly shows why merely matching playing strength is not sufficient to reproduce human decisions. We can prepare the interfaces and collect informative cases before Opera's NN, while keeping these training objectives separate. [McIlroy-Young et al., *Aligning Superhuman AI with Human Behavior*](https://arxiv.org/abs/2006.01855)

**Recommended development order and acceptance evidence**

First improve and measure king danger, defender roles and quiet tactical search. Preserve the sound Opera Game sacrifices while reducing the large optimism in the user's loss and finding the missed attack in the win. Test positions where the apparent sacrifice is refuted as well as positions where it works; use mirrored and independently selected held-out positions, rather than requiring one historical move in every opening.

Next add logging and a bounded opponent-model prototype. Validate reply prediction and confidence on withheld games before allowing it to change production move choice. Compare against the same engine without adaptation in paired matches with varied openings, both colors, identical search budgets, and several opponent families. Track game score, objective concession, tactical misses, and preservation of sound attacking choices. A gain against one weakened Stockfish configuration alone does not establish broad benefit.

Then build the value NN with these principles represented in its data and tests. Style should be expressed through sustainable activity, initiative and mating coordination, while the explicit root policy controls the optional practical gamble. Search speed must be measured with any new features: spending the entire move budget estimating the opponent can cost more tactical accuracy than adaptation gains.

**Artifacts and reproduction**

- [Annotated Opera Game PGN](games/morphy-opera-1858.pgn), with one comment for every White move and both evaluator choices.
- [Five user bot games](games/2026-09-21-bot-match-sample.pgn), retaining original moves, settings and evaluations plus source filenames/hashes.
- [All 17 historical comparisons](benchmarks/2026-09-21-morphy-opera.json).
- [437-position game screening](benchmarks/2026-09-21-bot-game-screen.json).
- [Deeper critical-position searches](benchmarks/2026-09-21-bot-game-critical.json), including the move-30 confirmation and static-term diagnostic.
- [Reusable audit script](../scripts/audit_style.py), using the existing pinned python-chess dependencies.

The tested Opera binary's SHA-256 is `a5e0b8932063d36d4af2ff0b8b1b9da4babbb1772f50f2757401bfe1c1fa8a33`, the engine build packaged at commit `47f0442eeb530912ab8de6f9c8a3d9a26cccf409`. The reference was local Stockfish **17.1**, full strength, one thread, 64 MB hash. Opera used one thread, 16 MB hash, Morphy style on or off as labeled. Each analyzed decision started a new UCI game to clear prior search state; it retained the board's move history. Reanalysis therefore need not exactly reproduce a warmed search during actual play.

Historical decisions used three seconds per Opera mode, 1 million reference nodes with three variations, and 500,000-node checks of the played/chosen candidates. Recent-game screening used 200,000 reference nodes per position. The five selected critical decisions used 8 million nodes/three variations, separate 4-million-node candidate comparisons, and Opera at three and ten seconds; move 30 received an additional 24-million-node restricted confirmation. Saved variations were independently replayed for legality. MultiPV comparisons use a common completed depth and exclude UCI lower/upper bounds; that does not turn finite-search evaluations into mathematical truth.

For example, from the repository with `scripts/requirements-uci.txt` installed:

```bash
python scripts/audit_style.py docs/games/morphy-opera-1858.pgn \
  --engine rust/target/release/opera-uci --stockfish /path/to/stockfish \
  --compare-standard --output /tmp/opera-historical-review.json

python scripts/audit_style.py docs/games/2026-09-21-bot-match-sample.pgn \
  --stockfish /path/to/stockfish --side both --reference-pv 1 \
  --reference-nodes 200000 --candidate-nodes 0 \
  --output /tmp/opera-game-screen.json

python scripts/audit_style.py docs/games/2026-09-21-bot-match-sample.pgn \
  --engine rust/target/release/opera-uci --stockfish /path/to/stockfish \
  --game-index 5 --ply 36 --reference-nodes 8000000 --candidate-nodes 4000000 \
  --candidate 36:bxa6 --candidate 36:Rfd1 --candidate 36:Rad1 \
  --movetime-ms 3000 --movetime-ms 10000 --compare-standard \
  --output /tmp/opera-critical-review.json
```

The reusable tool packages the audit procedure; its JSON envelope is more general than the initial saved records. Validation replayed all six saved PGNs and 612 recorded analysis variations (10,187 PV moves, including the tool smoke checks). The tool also passed actual-process checks from both sides of the final mating sequence, including Black's sole legal reply. Time-limited choices may differ with hardware and load. These are analysis checks, not a new engine test-suite run or a release of tuned playing behavior.
