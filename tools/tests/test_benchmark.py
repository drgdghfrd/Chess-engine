import sys
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools"))

from arena import GameResult, elo_diff, parse_uci_identity, result_for_engine
from rating.rating import elo_from_score, summarize


class RatingTests(unittest.TestCase):
    def test_balanced_score(self):
        s = summarize(10, 20, 10)
        self.assertEqual(s.games, 40)
        self.assertAlmostEqual(s.score, 0.5)
        self.assertAlmostEqual(s.elo_delta, 0.0)
        self.assertLess(s.score_ci95[0], 0.5)
        self.assertGreater(s.score_ci95[1], 0.5)

    def test_extreme_score_has_no_finite_elo(self):
        s = summarize(10, 0, 0)
        self.assertIsNone(s.elo_delta)
        self.assertIsNone(s.elo_ci95)

    def test_elo_monotonic(self):
        self.assertLess(elo_from_score(0.4), elo_from_score(0.6))

    def test_arena_elo_formula_uses_score_fraction(self):
        self.assertAlmostEqual(elo_diff(10.5, 24), -43.6578, places=3)


class ArenaAttributionTests(unittest.TestCase):
    def test_uci_identity(self):
        name, author = parse_uci_identity([
            "id name TestEngine 1.2",
            "id author Example",
            "option name Threads type spin default 1 min 1 max 8",
            "uciok",
        ])
        self.assertEqual(name, "TestEngine 1.2")
        self.assertEqual(author, "Example")

    def test_alternating_colors(self):
        white_win = GameResult("1-0", 1, ["e2e4"], "checkmate")
        black_win = GameResult("0-1", 1, ["e7e5"], "checkmate")
        draw = GameResult("1/2-1/2", 1, ["e2e4"], "draw")
        abort = GameResult("abort", 0, [], "stopped")

        self.assertEqual(result_for_engine(white_win, True), "win")
        self.assertEqual(result_for_engine(white_win, False), "loss")
        self.assertEqual(result_for_engine(black_win, True), "loss")
        self.assertEqual(result_for_engine(black_win, False), "win")
        self.assertEqual(result_for_engine(draw, True), "draw")
        self.assertEqual(result_for_engine(abort, True), "abort")


if __name__ == "__main__":
    unittest.main()
