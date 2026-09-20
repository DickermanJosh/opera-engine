#!/usr/bin/env python3
"""Repeatable UCI search sample. Install scripts/requirements-uci.txt first.

Report elapsed time, legal PVs and choices; this is not an Elo estimate.
Run each binary separately on an otherwise idle machine with identical flags.
"""
import argparse
import hashlib
import json
import pathlib
import platform
import time

import chess
import chess.engine


def opening(moves):
    board = chess.Board()
    for move in moves.split():
        board.push_san(move)
    return board.fen()


POSITIONS = {
    "start": chess.STARTING_FEN,
    "e4": opening("e4"),
    "italian_castling": opening("e4 e5 Nf3 Nc6 Bc4 Nf6 d3 Bc5"),
    "four_knights_pin": opening("e4 Nf6 Nc3 Nc6 d4 e5 d5 Ne7 Nf3 d6 Bb5+ Bd7 O-O"),
    "london": opening("d4 d5 Nf3 Nf6 Bf4 e6 e3 Bd6 Bg3 O-O Bd3"),
    "london_centre": opening("d4 Nf6 Bf4 d5 e3 c5 c3 Nc6 Nd2 e6 Ngf3 Bd6"),
    "kiwipete": "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",
    "rook_ending": "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",
    "promotion": "4k3/P7/8/8/8/8/7p/4K3 w - - 0 1",
    "knight_fork": "r3k3/pp3ppp/8/3N4/8/8/PPP2PPP/4K3 w q - 0 1",
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("engine", type=pathlib.Path)
    parser.add_argument("--depth", type=int, default=4)
    parser.add_argument("--movetime", type=float, help="Seconds, instead of fixed depth")
    parser.add_argument("--morphy", action="store_true")
    parser.add_argument("--repeats", type=int, default=1)
    parser.add_argument("--output", required=True, type=pathlib.Path)
    args = parser.parse_args()
    report = {"engine": str(args.engine.resolve()),
              "sha256": hashlib.sha256(args.engine.read_bytes()).hexdigest(),
              "platform": platform.platform(), "morphy": args.morphy,
              "limit": {"time": args.movetime} if args.movetime else {"depth": args.depth},
              "positions": []}
    limit = chess.engine.Limit(time=args.movetime) if args.movetime else chess.engine.Limit(depth=args.depth)
    with chess.engine.SimpleEngine.popen_uci(str(args.engine.resolve()), timeout=60) as engine:
        engine.configure({"Hash": 16, "Threads": 1, "MorphyStyle": args.morphy})
        for repeat in range(args.repeats):
            for name, fen in POSITIONS.items():
                board = chess.Board(fen)
                assert board.is_valid(), (name, board.status())
                start = time.monotonic()
                result = engine.play(board, limit, game=object(), info=chess.engine.INFO_ALL)
                elapsed = time.monotonic() - start
                assert result.move in board.legal_moves, (name, result.move)
                pv_board = board.copy()
                for move in result.info.get("pv", []):
                    assert move in pv_board.legal_moves, (name, move, pv_board.fen())
                    pv_board.push(move)
                row = {"name": name, "repeat": repeat, "fen": fen,
                       "move": result.move.uci(), "san": board.san(result.move),
                       "elapsed": elapsed, "depth": result.info.get("depth"),
                       "nodes": result.info.get("nodes"), "score": str(result.info.get("score")),
                       "pv": [m.uci() for m in result.info.get("pv", [])]}
                report["positions"].append(row)
                print(f"{name}: {row['san']} depth {row['depth']} {elapsed:.3f}s", flush=True)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
