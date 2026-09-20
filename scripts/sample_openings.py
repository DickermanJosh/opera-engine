#!/usr/bin/env python3
"""Sample early development against a fixed-node reference; these are unfinished games, not strength results."""
import argparse
import datetime
import hashlib
import json
import pathlib

import chess
import chess.engine
import chess.pgn

OPENINGS = {"King pawn": "e4", "London": "d4 Nf6 Bf4"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("engine", type=pathlib.Path)
    parser.add_argument("opponent", type=pathlib.Path)
    parser.add_argument("--movetime", type=float, default=1.0)
    parser.add_argument("--opponent-nodes", type=int, default=4000)
    parser.add_argument("--plies", type=int, default=24)
    parser.add_argument("--output", type=pathlib.Path, required=True, help="Prefix for JSON and PGN")
    args = parser.parse_args()
    rows, pgns = [], []
    with chess.engine.SimpleEngine.popen_uci(str(args.engine.resolve())) as engine, \
            chess.engine.SimpleEngine.popen_uci(str(args.opponent.resolve())) as opponent:
        engine.configure({"MorphyStyle": True, "Hash": 16, "Threads": 1})
        opponent.configure({"Hash": 16, "Threads": 1})
        for name, opening in OPENINGS.items():
            board = chess.Board()
            for san in opening.split():
                board.push_san(san)
            game_id, counts = object(), []
            while board.ply() < args.plies and not board.is_game_over(claim_draw=True):
                mover = opponent if board.turn else engine
                limit = chess.engine.Limit(nodes=args.opponent_nodes) if board.turn else chess.engine.Limit(time=args.movetime)
                result = mover.play(board, limit, game=game_id, info=chess.engine.INFO_ALL)
                assert result.move in board.legal_moves
                pv = board.copy()
                for move in result.info.get("pv", []):
                    assert move in pv.legal_moves
                    pv.push(move)
                board.push(result.move)
                if board.turn:
                    minors = sum(piece.color == chess.BLACK and piece.piece_type in (chess.KNIGHT, chess.BISHOP)
                                 and chess.square_rank(square) != 7 for square, piece in board.piece_map().items())
                    bishops = len(board.pieces(chess.BISHOP, chess.BLACK) & ~chess.SquareSet(chess.BB_RANK_8))
                    counts.append({"move": board.fullmove_number - 1, "developed_minors": minors, "bishops": bishops})
            game = chess.pgn.Game.from_board(board)
            game.headers.update({"Event": "Opera opening activity sample", "Opening": name,
                                 "Date": datetime.date.today().strftime("%Y.%m.%d"),
                                 "White": opponent.id.get("name", "Reference") + " at " + str(args.opponent_nodes) + " nodes",
                                 "Black": "Opera Morphy at " + str(args.movetime) + " seconds"})
            pgns.append(str(game))
            rows.append({"name": name, "opening": opening, "development": counts, "pgn": str(game)})
            print(str(game), flush=True)
    report = {"engine_sha256": hashlib.sha256(args.engine.read_bytes()).hexdigest(), "morphy": True,
              "movetime": args.movetime, "opponent_nodes": args.opponent_nodes, "plies_limit": args.plies, "games": rows}
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.with_suffix(".json").write_text(json.dumps(report, indent=2) + "\n")
    args.output.with_suffix(".pgn").write_text("\n\n".join(pgns) + "\n")


if __name__ == "__main__":
    main()
