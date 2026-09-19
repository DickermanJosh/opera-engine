#!/usr/bin/env python3
"""Check Opera bestmoves, ponder replies and every PV using Stockfish's perft.

Usage: python3 scripts/test_uci_oracle.py OPERA_BINARY STOCKFISH_BINARY
This checks independent chess-rule agreement, not playing strength or Elo.
"""
import argparse
import random
import re

from test_uci_process import Engine


class Oracle(Engine):
    def legal_moves(self, position):
        self.send(position)
        self.send('go perft 1')
        lines = self.until('Nodes searched:', timeout=5)
        return {
            match[1] for line in lines
            if (match := re.fullmatch(r'([a-h][1-8][a-h][1-8][qrbn]?): 1', line))
        }


def append_moves(position, moves):
    if not moves:
        return position
    separator = ' ' if ' moves ' in position else ' moves '
    return position + separator + ' '.join(moves)


def check_search(engine, oracle, position, counters, root=None):
    legal = oracle.legal_moves(position)
    if root is not None:
        assert root in legal, (position, root)
    engine.send(position)
    # A node cap bounds tactical trees; a completed shallow iteration normally
    # supplies a PV, while the legal fallback remains valid on immediate stops.
    engine.send('go depth 3 nodes 3000' + (f' searchmoves {root}' if root else ''))
    lines = engine.until('bestmove ', timeout=5)
    for line in lines:
        assert not line.startswith('info string ERROR'), (position, line)
        if ' pv ' in line:
            pv = line.split(' pv ', 1)[1].split()
            prefix = []
            for move in pv:
                allowed = legal if not prefix else oracle.legal_moves(append_moves(position, prefix))
                assert move in allowed, ('illegal PV', position, prefix, line)
                prefix.append(move)
                counters['pv_moves'] += 1
    result = lines[-1].split()
    best = result[1]
    if not legal:
        assert result == ['bestmove', '0000'], (position, result)
    else:
        assert best in legal, ('illegal bestmove', position, result)
        if root is not None:
            assert best == root, (position, result, root)
        if len(result) > 2:
            assert len(result) == 4 and result[2] == 'ponder', result
            replies = oracle.legal_moves(append_moves(position, [best]))
            assert result[3] in replies, ('illegal ponder', position, result)
            counters['ponder_replies'] += 1
    counters['positions'] += 1
    return legal, best


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('engine')
    parser.add_argument('stockfish')
    parser.add_argument('--plies', type=int, default=160)
    args = parser.parse_args()
    if args.plies < 0:
        parser.error('--plies must be nonnegative')
    engine, oracle = Engine(args.engine), Oracle(args.stockfish)
    counters = {'positions': 0, 'pv_moves': 0, 'ponder_replies': 0}
    try:
        for process in [engine, oracle]:
            process.send('uci')
            process.until('uciok', timeout=10)
            process.send('setoption name Hash value 16')
            process.send('isready')
            process.until('readyok', timeout=10)
        engine.send('setoption name Ponder value true')
        cases = [
            ('7k/6Q1/6K1/8/8/8/8/8 b - - 0 1', None),
            ('7k/5Q2/6K1/8/8/8/8/8 b - - 0 1', None),
            ('7k/P7/8/8/8/8/8/7K w - - 0 1', 'a7a8n'),
            ('7k/8/8/8/8/8/p7/7K b - - 0 1', 'a2a1q'),
            ('r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1', 'e1g1'),
            ('r3k2r/8/8/8/8/8/8/R3K2R b KQkq - 0 1', 'e8c8'),
            ('7k/8/8/3pP3/8/8/8/7K w - d6 0 1', 'e5d6'),
            ('7k/8/8/8/3Pp3/8/8/7K b - d3 0 1', 'e4d3'),
            ('7k/8/8/8/8/8/q7/R6K w - - 0 1', None),
            ('r6k/Q7/8/8/8/8/8/7K b - - 0 1', None),
        ]
        for fen, root in cases:
            position = 'position fen ' + fen
            check_search(engine, oracle, position, counters, root)
            if root:
                check_search(engine, oracle, append_moves(position, [root]), counters)

        # Fixed seed and alternating engine/random moves exercise reproducible,
        # varied legal histories with both evaluators and both colors to move.
        randomizer = random.Random(20260918)
        moves = []
        for ply in range(args.plies):
            if ply % 40 == 0:
                moves = []
                engine.send('ucinewgame')
                engine.send('setoption name MorphyStyle value ' +
                            ('true' if (ply // 40) % 2 else 'false'))
            position = append_moves('position startpos', moves)
            legal, best = check_search(engine, oracle, position, counters)
            if legal:
                moves.append(best if ply % 3 == 0 else randomizer.choice(sorted(legal)))
            else:
                moves = []
                engine.send('ucinewgame')
        print('Independent Stockfish validation passed:', counters)
    finally:
        engine.close()
        oracle.close()


if __name__ == '__main__':
    main()
