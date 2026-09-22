#!/usr/bin/env python3
"""Compare PGN decisions with Opera's two evaluators and full-strength Stockfish.

Uses the dependencies in requirements-uci.txt. This is an offline diagnostic,
not a rating estimator, a production opponent model, or a move-match test.
Scores are White-relative; mate scores remain separate from centipawns.
"""

import argparse
import contextlib
import datetime
import hashlib
import json
from pathlib import Path

import chess
import chess.engine
import chess.pgn


def positive_int(value):
    number = int(value)
    if number <= 0:
        raise argparse.ArgumentTypeError("must be positive")
    return number


def pack(board, info):
    replay = board.copy()
    sans, ucis = [], []
    for move in info.get("pv", []):
        if move not in replay.legal_moves:
            raise ValueError(f"Illegal PV move {move} in {replay.fen()}")
        sans.append(replay.san(move))
        ucis.append(move.uci())
        replay.push(move)
    score = info["score"].white()
    return {
        "cp": score.score(), "mate": score.mate(), "depth": info.get("depth"),
        "nodes": info.get("nodes"), "time": info.get("time"),
        "pv_san": sans, "pv_uci": ucis,
    }


def analyse(engine, board, limit, multipv=1, roots=None):
    """Return the deepest common-depth, exact-score set, excluding bounds."""
    count = min(multipv, len(roots) if roots is not None else board.legal_moves.count())
    if count == 0:
        return []
    if count > 1 and "MultiPV" not in engine.options:
        raise ValueError(f"{engine.id} does not advertise MultiPV")
    kwargs = {"multipv": count} if "MultiPV" in engine.options else {}
    if roots is not None:
        kwargs["root_moves"] = roots
    snapshots = {}
    # Each decision starts a new UCI game, preventing earlier positions from
    # warming the transposition table. Board move history is still transmitted.
    with engine.analysis(board, limit, game=object(), info=chess.engine.INFO_ALL, **kwargs) as stream:
        for info in stream:
            if ("score" in info and info.get("pv") and
                    not info.get("lowerbound") and not info.get("upperbound")):
                snapshots.setdefault(info.get("depth", 0), {})[info.get("multipv", 1)] = dict(info)
    complete = [depth for depth, rows in snapshots.items()
                if all(index in rows for index in range(1, count + 1))]
    if not complete:
        raise RuntimeError(f"No complete {count}-PV iteration at {board.fen()}")
    rows = snapshots[max(complete)]
    return [pack(board, rows[index]) for index in range(1, count + 1)]


def identity(engine, binary):
    return {"uci": engine.id, "sha256": hashlib.sha256(binary.read_bytes()).hexdigest()}


def read_games(path):
    games = []
    with path.open(encoding="utf-8-sig") as stream:
        while (game := chess.pgn.read_game(stream)) is not None:
            if game.errors:
                raise ValueError(f"PGN parse errors: {game.errors}")
            board = game.board()
            if not board.is_valid():
                raise ValueError(f"Invalid starting position: {board.fen()}")
            for move in game.mainline_moves():
                if move not in board.legal_moves:
                    raise ValueError(f"Illegal PGN move {move} in {board.fen()}")
                board.push(move)
            if "FinalFEN" in game.headers and board.fen(en_passant="fen") != game.headers["FinalFEN"]:
                raise ValueError("Exported FinalFEN does not match its moves")
            if board.is_game_over() and game.headers["Result"] != board.result():
                raise ValueError("PGN result does not match the terminal board")
            games.append(game)
    if not games:
        raise ValueError("PGN contains no games")
    return games


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("pgn", type=Path)
    parser.add_argument("--engine", type=Path, help="Opera UCI binary; omit for reference-only screening")
    parser.add_argument("--stockfish", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--side", choices=["white", "black", "both"], default="white")
    parser.add_argument("--game-index", type=positive_int, help="1-based game number; default all")
    parser.add_argument("--ply", type=int, action="append", help="0-based half-move index; repeatable")
    parser.add_argument("--movetime-ms", type=positive_int, action="append", help="Repeat for multiple budgets; default 3000")
    parser.add_argument("--compare-standard", action="store_true")
    parser.add_argument("--reference-nodes", type=positive_int, default=1000000)
    parser.add_argument("--reference-pv", type=positive_int, default=3)
    parser.add_argument("--candidate-nodes", type=int, default=500000, help="0 disables candidate checks")
    parser.add_argument("--candidate", action="append", default=[], metavar="PLY:SAN",
                        help="Explicit comparison set for a ply, plus the played move and reference best")
    args = parser.parse_args()
    if args.candidate_nodes < 0 or any(ply < 0 for ply in args.ply or []):
        parser.error("node limits and ply indices cannot be negative")
    if args.compare_standard and not args.engine:
        parser.error("--compare-standard requires --engine")
    extra = {}
    for value in args.candidate:
        try:
            index, san = value.split(":", 1)
            index = int(index)
            if index < 0 or not san:
                raise ValueError
        except ValueError:
            parser.error("--candidate must be a nonnegative ply index followed by :SAN")
        extra.setdefault(index, []).append(san)
    games = read_games(args.pgn)
    if args.game_index and args.game_index > len(games):
        parser.error("--game-index is outside the input PGN")
    times = args.movetime_ms or [3000]
    report = {
        "created_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "input": args.pgn.name, "input_sha256": hashlib.sha256(args.pgn.read_bytes()).hexdigest(),
        "score_perspective": "white", "threads": 1, "reference_hash_mb": 64,
        "opera_hash_mb": 16, "reference_nodes": args.reference_nodes,
        "reference_pv": args.reference_pv, "candidate_nodes": args.candidate_nodes,
        "movetime_ms": times if args.engine else [], "cold_game_per_position": True,
        "selection": {"side": args.side, "game_index": args.game_index, "plies": args.ply},
        "games": [], "complete": False,
    }
    args.output.parent.mkdir(parents=True, exist_ok=True)
    with contextlib.ExitStack() as stack:
        sf = stack.enter_context(chess.engine.SimpleEngine.popen_uci(str(args.stockfish.resolve())))
        sf.configure({"Threads": 1, "Hash": 64, "UCI_LimitStrength": False, "Skill Level": 20})
        report["reference"] = identity(sf, args.stockfish)
        engines = {}
        if args.engine:
            for label, morphy in [("morphy", True)] + ([("standard", False)] if args.compare_standard else []):
                engine = stack.enter_context(chess.engine.SimpleEngine.popen_uci(str(args.engine.resolve())))
                engine.configure({"Threads": 1, "Hash": 16, "MorphyStyle": morphy})
                engines[label] = engine
            report["engine"] = identity(engines["morphy"], args.engine)
        for index, game in enumerate(games, 1):
            if args.game_index and index != args.game_index:
                continue
            record = {"game_index": index, "headers": dict(game.headers), "positions": []}
            report["games"].append(record)
            node, board = game, game.board()
            for ply, move in enumerate(game.mainline_moves()):
                side = "white" if board.turn else "black"
                selected = (args.side in ("both", side) and (args.ply is None or ply in args.ply))
                if selected:
                    item = {"ply": ply, "turn": side, "move_number": board.fullmove_number,
                            "fen": board.fen(en_passant="fen"), "played": board.san(move),
                            "uci": move.uci(), "pre_move_comment": node.comment}
                    item["reference"] = analyse(sf, board, chess.engine.Limit(nodes=args.reference_nodes), args.reference_pv)
                    choices = {move.uci()}
                    for label, engine in engines.items():
                        for milliseconds in times:
                            row = analyse(engine, board, chess.engine.Limit(time=milliseconds / 1000))[0]
                            item[f"{label}_{milliseconds}ms"] = row
                            choices.add(row["pv_uci"][0])
                    if args.candidate_nodes:
                        limit = chess.engine.Limit(nodes=args.candidate_nodes)
                        if ply in extra:
                            choices = {move.uci()} | {board.parse_san(san).uci() for san in extra[ply]}
                            choices.add(item["reference"][0]["pv_uci"][0])
                            roots = [chess.Move.from_uci(uci) for uci in sorted(choices)]
                            item["candidates"] = analyse(sf, board, limit, len(roots), roots)
                        else:
                            item["candidate_reference"] = {
                                uci: analyse(sf, board, limit, roots=[chess.Move.from_uci(uci)])[0]
                                for uci in sorted(choices)
                            }
                    record["positions"].append(item)
                    args.output.write_text(json.dumps(report, indent=2) + "\n")
                    print(f"game {index}, ply {ply}: {item['played']}", flush=True)
                board.push(move)
                node = node.next()
            record["final_fen"] = board.fen(en_passant="fen")
        if not any(record["positions"] for record in report["games"]):
            parser.error("selection contained no played moves")
        report["complete"] = True
        args.output.write_text(json.dumps(report, indent=2) + "\n")


if __name__ == "__main__":
    main()
