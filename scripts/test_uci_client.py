#!/usr/bin/env python3
"""Exercise Opera through python-chess, with optional timed Stockfish games.

Install scripts/requirements-uci.txt, then pass OPERA_BINARY [STOCKFISH_BINARY].
This is protocol/client acceptance, not an Elo or GUI rendering test.
"""
import argparse
import asyncio
import json
import logging
import time

import chess
import chess.engine
import chess.pgn


class ProtocolLog(logging.Handler):
    def __init__(self):
        super().__init__()
        self.ponder_hits = 0
        self.warnings = []

    def emit(self, record):
        message = record.getMessage()
        if message.endswith('<< ponderhit'):
            self.ponder_hits += 1
        if record.levelno >= logging.WARNING:
            self.warnings.append(message)


async def bounded(operation):
    return await asyncio.wait_for(operation, timeout=5)


def validate_result(board, result):
    assert result.move in board.legal_moves, (board.fen(), result)
    after = board.copy()
    after.push(result.move)
    if result.ponder is not None:
        assert result.ponder in after.legal_moves, (board.fen(), result)
    variation = board.copy()
    for move in result.info.get('pv', []):
        assert move in variation.legal_moves, (board.fen(), result.info)
        variation.push(move)


async def client_checks(engine, log):
    await bounded(engine.configure({'Hash': 16, 'Threads': 1, 'Move Overhead': 10}))
    await bounded(engine.ping())
    game = object()
    board = chess.Board()
    first = await bounded(engine.play(board, chess.engine.Limit(depth=2),
                                      game=game, ponder=True, info=chess.engine.INFO_ALL))
    validate_result(board, first)
    assert first.ponder is not None, 'Expected a reply from the depth-two starting PV'
    board.push(first.move)
    board.push(first.ponder)
    second = await bounded(engine.play(board, chess.engine.Limit(depth=2),
                                       game=game, ponder=True, info=chess.engine.INFO_ALL))
    validate_result(board, second)
    assert log.ponder_hits >= 1, 'External client did not exercise ponderhit'
    board.push(second.move)
    # A different opponent reply forces the client to stop the ponder search
    # and send a replacement position/go sequence.
    alternate = next(move for move in board.legal_moves if move != second.ponder)
    board.push(alternate)
    third = await bounded(engine.play(board, chess.engine.Limit(depth=2),
                                      game=game, info=chess.engine.INFO_ALL))
    validate_result(board, third)

    analysis = await bounded(engine.analysis(chess.Board(), info=chess.engine.INFO_ALL))
    await bounded(analysis.get())
    analysis.stop()
    stopped = await bounded(analysis.wait())
    assert stopped.move in chess.Board().legal_moves
    await bounded(engine.ping())

    for fen, move in [
        ('7k/P7/8/8/8/8/8/7K w - - 0 1', 'a7a8n'),
        ('r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1', 'e8c8'),
        ('7k/8/8/3pP3/8/8/8/7K w - d6 0 1', 'e5d6'),
    ]:
        position = chess.Board(fen)
        result = await bounded(engine.play(position, chess.engine.Limit(depth=2),
                                           root_moves=[chess.Move.from_uci(move)]))
        validate_result(position, result)
        assert result.move.uci() == move
    terminal = chess.Board('7k/6Q1/6K1/8/8/8/8/8 b - - 0 1')
    result = await bounded(engine.play(terminal, chess.engine.Limit(depth=2)))
    assert not result.move


async def timed_game(opera, opponent, opera_color, morphy):
    await bounded(opera.configure({'MorphyStyle': morphy}))
    board = chess.Board()
    game = object()
    clocks = {chess.WHITE: 3.0, chess.BLACK: 3.0}
    increment = 0.05
    for _ in range(240):
        if board.is_game_over(claim_draw=True):
            break
        side = board.turn
        player = opera if side == opera_color else opponent
        limit = chess.engine.Limit(white_clock=clocks[chess.WHITE],
                                   black_clock=clocks[chess.BLACK],
                                   white_inc=increment, black_inc=increment)
        if player is opponent:
            limit.time = 0.02
        started = time.monotonic()
        result = await bounded(player.play(board, limit, game=game,
                                           ponder=player is opera,
                                           info=chess.engine.INFO_ALL))
        clocks[side] -= time.monotonic() - started
        assert clocks[side] > 0, ('Time forfeit', side, board.fen(), clocks)
        validate_result(board, result)
        board.push(result.move)
        clocks[side] += increment
    outcome = board.outcome(claim_draw=True)
    assert outcome is not None, 'Timed game exceeded the 240-ply acceptance bound'
    pgn = chess.pgn.Game.from_board(board)
    pgn.headers['Event'] = 'Opera UCI client acceptance'
    pgn.headers['White'] = 'Opera' if opera_color else 'Stockfish'
    pgn.headers['Black'] = 'Stockfish' if opera_color else 'Opera'
    pgn.headers['TimeControl'] = '3+0.05'
    pgn.headers['StockfishMoveTime'] = '0.02'
    pgn.headers['Result'] = outcome.result()
    pgn.headers['Termination'] = outcome.termination.name
    pgn.headers['MorphyStyle'] = str(morphy).lower()
    return {'opera_color': 'white' if opera_color else 'black', 'morphy': morphy,
            'plies': board.ply(), 'result': outcome.result(),
            'termination': outcome.termination.name}, str(pgn)


async def run(args, log):
    processes = []
    games, pgns = [], []
    cleanup_errors = []
    try:
        transport, engine = await bounded(chess.engine.popen_uci(args.engine))
        processes.append((transport, engine))
        await client_checks(engine, log)
        if args.stockfish:
            transport, opponent = await bounded(chess.engine.popen_uci(args.stockfish))
            processes.append((transport, opponent))
            await bounded(opponent.configure({'Threads': 1, 'Hash': 16}))
            for color, morphy in [(chess.WHITE, False), (chess.BLACK, True)]:
                game, pgn = await timed_game(engine, opponent, color, morphy)
                games.append(game)
                pgns.append(pgn)
        assert not log.warnings, log.warnings
    finally:
        for transport, engine in reversed(processes):
            try:
                await bounded(engine.quit())
                if transport.get_returncode() != 0:
                    cleanup_errors.append(f'Nonzero exit: {transport.get_returncode()}')
            except Exception as error:
                cleanup_errors.append(str(error))
            finally:
                transport.close()
    assert not cleanup_errors, cleanup_errors
    if args.pgn:
        with open(args.pgn, 'w', encoding='utf-8') as output:
            output.write('\n\n'.join(pgns) + '\n')
    print('External UCI client acceptance passed:', json.dumps({
        'chess_version': chess.__version__, 'ponder_hits': log.ponder_hits, 'games': games}))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('engine')
    parser.add_argument('stockfish', nargs='?')
    parser.add_argument('--pgn', help='Write completed acceptance games here')
    args = parser.parse_args()
    log = ProtocolLog()
    logger = logging.getLogger('chess.engine')
    logger.setLevel(logging.DEBUG)
    logger.addHandler(log)
    asyncio.run(asyncio.wait_for(run(args, log), timeout=120))


if __name__ == '__main__':
    main()
