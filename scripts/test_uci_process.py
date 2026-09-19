#!/usr/bin/env python3
"""Bounded, real-process UCI regression checks; pass path to opera-uci."""
import queue
import os
import signal
import subprocess
import sys
import threading
import time
import unittest

BINARY = sys.argv.pop(1) if __name__ == '__main__' else None

class Engine:
    def __init__(self, binary=None):
        self.p = subprocess.Popen([binary or BINARY], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                  stderr=subprocess.DEVNULL, text=True, bufsize=1)
        self.lines = queue.Queue()
        def read():
            for line in self.p.stdout:
                self.lines.put(line.strip())
        threading.Thread(target=read, daemon=True).start()
    def send(self, text):
        self.p.stdin.write(text + '\n'); self.p.stdin.flush()
    def until(self, prefix, timeout=2):
        end = time.monotonic() + timeout
        lines = []
        while time.monotonic() < end:
            try: line = self.lines.get(timeout=max(.001, end-time.monotonic()))
            except queue.Empty: break
            lines.append(line)
            if line.startswith(prefix): return lines
        raise AssertionError(f'No {prefix!r}; got {lines}; exit={self.p.poll()}')
    def close(self):
        if self.p.poll() is None:
            try: self.send('quit'); self.p.wait(timeout=1)
            except (subprocess.TimeoutExpired, BrokenPipeError): self.p.kill(); self.p.wait()
        try: self.p.stdin.close()
        except BrokenPipeError: pass
        self.p.stdout.close()

class Protocol(unittest.TestCase):
    def setUp(self): self.e = Engine()
    def tearDown(self): self.e.close()
    def ready(self): self.e.send('isready'); self.e.until('readyok')
    def search(self, position, go='go depth 2'):
        self.e.send(position); self.e.send(go)
        lines = self.e.until('bestmove ')
        self.assertTrue(any(x.startswith('info depth ') for x in lines), lines)
        return lines[-1].split()[1]
    def test_handshake_is_command_driven(self):
        time.sleep(.1)
        self.assertTrue(self.e.lines.empty())
        self.e.send('uci'); lines = self.e.until('uciok')
        self.assertTrue(any(x.startswith('id name Opera') for x in lines))
        self.assertTrue(all(x.startswith(('id ', 'option ', 'uciok')) for x in lines))
        self.ready(); self.ready()
    def test_black_and_nonstart(self):
        move = self.search('position startpos moves e2e4')
        self.assertIn(move, [f'{f}7{f}{rank}' for f in 'abcdefgh' for rank in '65'] + ['b8a6','b8c6','g8f6','g8h6'])
        move = self.search('position fen 7k/8/8/8/8/8/6r1/7K w - - 0 1')
        self.assertEqual(move, 'h1g2')

    def test_both_sides_capture_undefended_queen(self):
        for style in ['false', 'true']:
            self.e.send('setoption name MorphyStyle value ' + style)
            for fen, expected in [
                ('7k/8/8/8/8/8/q7/R6K w - - 0 1', 'a1a2'),
                ('r6k/Q7/8/8/8/8/8/7K b - - 0 1', 'a8a7'),
            ]:
                with self.subTest(style=style, fen=fen):
                    self.assertEqual(self.search('position fen ' + fen, 'go depth 1'), expected)
    def test_terminal_positions(self):
        self.assertEqual(self.search('position fen 7k/6Q1/6K1/8/8/8/8/8 b - - 0 1'), '0000')
        self.assertEqual(self.search('position fen 7k/5Q2/6K1/8/8/8/8/8 b - - 0 1'), '0000')
    def test_invalid_position_is_transactional(self):
        self.e.send('position fen 7k/8/8/8/8/8/6r1/7K w - - 0 1')
        for bad in ['position startpos moves e2e4 e7e3', 'position fen 8/8/8/8/8/8/8/8 w - - 0 1', 'position fen 99/8/8/8/8/8/8/K6k w - - 0 1']:
            self.e.send(bad); self.e.until('info string ERROR')
            self.e.send('go depth 1'); self.assertEqual(self.e.until('bestmove ')[-1].split()[1], 'h1g2')
    def test_movetime_and_stop(self):
        self.e.send('position startpos'); start=time.monotonic(); self.e.send('go movetime 100')
        self.e.until('bestmove ', 1); self.assertLess(time.monotonic()-start, .5)
        self.e.send('go infinite'); time.sleep(.05); self.ready()
        start=time.monotonic(); self.e.send('stop'); self.e.until('bestmove ', .5)
        self.assertLess(time.monotonic()-start, .5)
        self.e.send('stop'); self.ready()
    def test_ponder_and_quit(self):
        self.e.send('position startpos'); self.e.send('go ponder depth 1'); time.sleep(.1)
        pending=[]
        while not self.e.lines.empty(): pending.append(self.e.lines.get_nowait())
        self.assertFalse(any(x.startswith('bestmove ') for x in pending))
        self.e.send('ponderhit'); self.e.until('bestmove ')
        self.e.send('go infinite'); time.sleep(.03); start=time.monotonic(); self.e.send('quit')
        self.e.p.wait(timeout=.5); self.assertLess(time.monotonic()-start,.5)
    def test_malformed_and_newgame(self):
        self.e.send('go depth bananas'); self.e.until('info string ERROR'); self.ready()
        self.e.send('setoption name Hash value 8'); self.e.send('setoption name Clear Hash'); self.ready()
        self.e.send('ucinewgame'); move=self.search('position startpos','go nodes 100')
        self.assertNotEqual(move,'0000')

    def test_special_moves_and_searchmoves(self):
        cases = [
            ('position fen 7k/P7/8/8/8/8/8/7K w - - 0 1', 'a7a8n'),
            ('position fen 7k/8/8/8/8/8/p7/7K b - - 0 1', 'a2a1q'),
            ('position fen r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1', 'e1g1'),
            ('position fen 7k/8/8/3pP3/8/8/8/7K w - d6 0 1', 'e5d6'),
            ('position fen 7k/8/8/8/8/8/8/K7 w - - 0 1', 'a1a2'),
        ]
        for position,move in cases:
            self.assertEqual(self.search(position, f'go depth 1 searchmoves {move}'), move)
            self.e.send(position+' moves '+move); self.ready()
        self.e.send('position startpos'); self.e.send('go searchmoves e2e5 depth 1')
        self.e.until('info string ERROR'); self.ready()
    def test_repeated_searches_and_burst_commands(self):
        for _ in range(12):
            self.e.send('position startpos'); self.e.send('go infinite'); self.e.send('stop')
            lines=self.e.until('bestmove ',1)
            self.assertNotEqual(lines[-1].split()[1],'0000')
            self.e.send('stop'); self.ready()
        self.e.send('\n'.join(['isready']*100))
        for _ in range(100): self.e.until('readyok')
        self.e.send('go infinite'); time.sleep(.02)
        self.e.send('position startpos moves e2e4'); self.e.until('bestmove ')
        self.e.send('go depth 1'); move=self.e.until('bestmove ')[-1].split()[1]
        self.assertIn(move[1], '78')
    def test_input_boundaries_and_clock_limits(self):
        self.e.send('x'*12000); self.e.until('info string ERROR'); self.ready()
        self.e.send('go depth 4294967296'); self.e.until('info string ERROR'); self.ready()
        self.e.send('position startpos moves e2e4')
        start=time.monotonic(); self.e.send('go wtime 100000 btime 50 binc 0 movestogo 1')
        self.e.until('bestmove ',.5); self.assertLess(time.monotonic()-start,.5)
        self.e.send('go infinite'); self.e.p.stdin.close(); self.e.p.wait(timeout=.5)
    def test_invalid_options_and_position_tail(self):
        for option in ['Hash value 0','Hash value 999999','Threads value 4','Ponder value maybe','MorphyStyle value perhaps','TacticalDepth value 5']:
            self.e.send('setoption name '+option); self.e.until('info string ERROR'); self.ready()
        self.e.send('position startpos garbage'); self.e.until('info string ERROR'); self.ready()

    def test_infinite_and_ponder_respect_node_limits(self):
        for mode in ['infinite', 'ponder']:
            with self.subTest(mode=mode):
                self.e.send('position startpos')
                self.e.send(f'go {mode} nodes 1 searchmoves e2e4')
                time.sleep(.05)
                self.e.send('isready')
                pending = self.e.until('readyok')
                self.assertFalse(any(x.startswith('bestmove ') for x in pending), pending)
                self.e.send('stop')
                lines = self.e.until('bestmove ')
                self.assertEqual(lines[-1], 'bestmove e2e4')
                info = [x.split() for x in pending + lines if x.startswith('info depth ')]
                self.assertTrue(info)
                self.assertTrue(all(int(x[x.index('nodes') + 1]) <= 1 for x in info), info)

    def test_depth_and_mate_use_the_tighter_limit(self):
        for limits, maximum in [('depth 4 mate 1', 2), ('mate 3 depth 1', 1)]:
            with self.subTest(limits=limits):
                self.e.send('position startpos')
                self.e.send('go ' + limits)
                lines = self.e.until('bestmove ')
                depths = [int(x.split()[2]) for x in lines if x.startswith('info depth ')]
                self.assertTrue(depths)
                self.assertLessEqual(max(depths), maximum)

    def test_searchmoves_normalization_and_validation(self):
        self.assertEqual(self.search('position startpos', 'go searchmoves E2E4 depth 1'), 'e2e4')
        self.e.send('go infinite')
        for bad in ['go searchmoves depth 1', 'go searchmoves e2e4 searchmoves d2d4 depth 1',
                    'go searchmoves e2e5 depth 1']:
            self.e.send(bad)
            lines = self.e.until('info string ERROR')
            self.assertFalse(any(x.startswith('bestmove ') for x in lines), lines)
        self.e.send('stop')
        self.e.until('bestmove ')
        self.e.send('isready')
        self.assertFalse(any(x.startswith('bestmove ') for x in self.e.until('readyok')))

    def test_multiword_option_spacing_and_button_validation(self):
        for option in ['Move\t Overhead value 0', 'Clear   Hash']:
            self.e.send('setoption name ' + option)
            self.e.send('isready')
            lines = self.e.until('readyok')
            self.assertFalse(any(x.startswith('info string ERROR') for x in lines), lines)
        for option in ['Clear Hash value 1', 'Clear Hash value']:
            self.e.send('setoption name ' + option)
            self.e.until('info string ERROR')
            self.ready()

    def test_ponder_option_emits_reply_from_pv(self):
        for enabled in ['true', 'false']:
            self.e.send('setoption name Ponder value ' + enabled)
            self.e.send('position startpos')
            self.e.send('go depth 2')
            lines = self.e.until('bestmove ')
            best = lines[-1].split()
            pv = [x.split(' pv ', 1)[1].split() for x in lines if ' pv ' in x][-1]
            self.assertGreaterEqual(len(pv), 2)
            expected = ['bestmove', pv[0]]
            if enabled == 'true':
                expected += ['ponder', pv[1]]
            self.assertEqual(best, expected)

    def test_ponder_clock_starts_at_ponderhit(self):
        self.e.send('setoption name Move Overhead value 0')
        self.e.send('position startpos')
        self.e.send('go ponder movetime 100')
        time.sleep(.15)
        self.e.send('isready')
        self.assertFalse(any(x.startswith('bestmove ') for x in self.e.until('readyok')))
        start = time.monotonic()
        self.e.send('ponderhit')
        self.e.until('bestmove ', 1)
        self.assertGreater(time.monotonic() - start, .02)
        self.ready()

    def test_closed_output_cancels_worker(self):
        p = subprocess.Popen([BINARY], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                             stderr=subprocess.DEVNULL, text=True)
        try:
            p.stdout.close()
            p.stdin.write('position startpos\ngo infinite\n')
            p.stdin.flush()
            self.assertEqual(p.wait(timeout=2), 1)
        finally:
            if p.poll() is None:
                p.kill(); p.wait()
            try: p.stdin.close()
            except BrokenPipeError: pass

    def test_output_backpressure_has_bounded_exit(self):
        p = subprocess.Popen([BINARY], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                             stderr=subprocess.DEVNULL, text=True)
        def flood():
            try:
                p.stdin.write('isready\n' * 50000)
                p.stdin.flush()
            except BrokenPipeError:
                pass
        writer = threading.Thread(target=flood, daemon=True)
        writer.start()
        try:
            # Intentionally leave stdout unread. The five-second write timeout
            # must end the process even while Tokio's OS write is still blocked.
            self.assertEqual(p.wait(timeout=8), 1)
        finally:
            if p.poll() is None:
                p.kill(); p.wait()
            writer.join(timeout=1)
            try: p.stdin.close()
            except BrokenPipeError: pass
            p.stdout.close()

    @unittest.skipUnless(os.name == 'posix', 'POSIX signals')
    def test_signals_stop_search_and_exit_cleanly(self):
        for sig in [signal.SIGTERM, signal.SIGINT]:
            with self.subTest(signal=sig):
                try:
                    self.e.send('position startpos')
                    self.e.send('go infinite')
                    self.ready()
                    self.e.p.send_signal(sig)
                    self.e.until('bestmove ', 1)
                    self.assertEqual(self.e.p.wait(timeout=1), 0)
                finally:
                    self.e.close()
                    self.e = Engine()

if __name__ == '__main__': unittest.main()
