#!/usr/bin/env python3
"""Paired UCI games against a saved engine; output legal PGNs and a small-sample summary."""
import argparse
import hashlib
import json
import pathlib
from contextlib import ExitStack

import chess
import chess.engine
import chess.pgn

OPENINGS = {
    "Open game": "e4 e5 Nf3 Nc6 Bc4 Nf6 d3 Bc5",
    "London": "d4 d5 Nf3 Nf6 Bf4 e6 e3 Bd6",
    "Queen's gambit": "d4 d5 c4 e6 Nc3 Nf6 Nf3 Be7",
    "Sicilian": "e4 c5 Nf3 d6 d4 cxd4 Nxd4 Nf6",
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("candidate", type=pathlib.Path)
    parser.add_argument("baseline", type=pathlib.Path)
    parser.add_argument("--movetime", type=float, default=0.1)
    parser.add_argument("--morphy", action="store_true", help="Candidate's style")
    parser.add_argument("--baseline-morphy", action="store_true", help="Enable Morphy style in the saved baseline too")
    parser.add_argument("--baseline-name", default="Baseline")
    parser.add_argument("--max-plies", type=int, default=240)
    parser.add_argument("--openings-file", type=pathlib.Path, help="JSON object mapping opening names to SAN move strings")
    parser.add_argument("--output", type=pathlib.Path, required=True, help="Output prefix for .pgn and .json")
    args = parser.parse_args()
    openings = OPENINGS
    if args.openings_file:
        try:
            openings = json.loads(args.openings_file.read_text())
        except (OSError, json.JSONDecodeError) as error:
            parser.error(str(error))
        if not isinstance(openings, dict) or not openings or not all(
                isinstance(name, str) and isinstance(moves, str) for name, moves in openings.items()):
            parser.error("--openings-file must contain a nonempty object of opening names and SAN strings")
    rows, games = [], []
    with ExitStack() as stack:
        engines = [stack.enter_context(chess.engine.SimpleEngine.popen_uci(str(path.resolve()), timeout=10))
                   for path in (args.candidate, args.baseline)]
        for index, engine in enumerate(engines):
            engine.configure({"Hash": 16, "Threads": 1, "MorphyStyle": args.morphy if index == 0 else args.baseline_morphy})
        for name, opening in openings.items():
            for candidate_color in (chess.WHITE, chess.BLACK):
                board = chess.Board()
                for san in opening.split():
                    board.push_san(san)
                game_id = object()
                castling = []
                for _ in range(args.max_plies):
                    if board.is_game_over(claim_draw=True):
                        break
                    engine = engines[0 if board.turn == candidate_color else 1]
                    result = engine.play(board, chess.engine.Limit(time=args.movetime),
                                         game=game_id, info=chess.engine.INFO_ALL)
                    assert result.move in board.legal_moves, (name, board.fen(), result.move)
                    pv = board.copy()
                    for move in result.info.get("pv", []):
                        assert move in pv.legal_moves, (name, pv.fen(), move)
                        pv.push(move)
                    if board.is_castling(result.move):
                        castling.append({"side": "candidate" if board.turn == candidate_color else "baseline",
                                         "move": board.fullmove_number, "san": board.san(result.move)})
                    board.push(result.move)
                outcome = board.outcome(claim_draw=True)
                result_text = outcome.result() if outcome else "*"
                candidate_result = ("draw" if outcome.winner is None else
                                    "win" if outcome.winner == candidate_color else "loss") if outcome else "unfinished"
                game = chess.pgn.Game.from_board(board)
                game.headers.update({"Event": "Opera core paired regression match", "Opening": name,
                                     "White": "Candidate" if candidate_color else args.baseline_name,
                                     "Black": args.baseline_name if candidate_color else "Candidate",
                                     "Result": result_text, "MoveTime": str(args.movetime),
                                     "CandidateMorphy": str(args.morphy).lower(),
                                     "BaselineMorphy": str(args.baseline_morphy).lower(),
                                     "Termination": outcome.termination.name if outcome else "Ply limit (not adjudicated)"})
                games.append(str(game))
                row = {"opening": name, "candidate_color": "white" if candidate_color else "black",
                       "result": result_text, "candidate_result": candidate_result, "plies": board.ply(),
                       "termination": game.headers["Termination"], "castling": castling}
                rows.append(row)
                print(json.dumps(row), flush=True)
    report = {"candidate_sha256": hashlib.sha256(args.candidate.read_bytes()).hexdigest(),
              "baseline_sha256": hashlib.sha256(args.baseline.read_bytes()).hexdigest(),
              "morphy": args.morphy, "baseline_morphy": args.baseline_morphy, "baseline_name": args.baseline_name, "movetime": args.movetime,
              "openings": openings,
              "summary": {result: sum(row["candidate_result"] == result for row in rows)
                          for result in ("win", "draw", "loss", "unfinished")}, "games": rows}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.with_suffix(".pgn").write_text("\n\n".join(games) + "\n")
    args.output.with_suffix(".json").write_text(json.dumps(report, indent=2) + "\n")
    print(json.dumps(report["summary"]))


if __name__ == "__main__":
    main()
