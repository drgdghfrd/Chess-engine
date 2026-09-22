#!/usr/bin/env python3
"""Analyze Arena JSONL output and verify basic benchmark invariants."""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

TOOLS_ROOT = Path(__file__).resolve().parents[1]
if str(TOOLS_ROOT) not in sys.path:
    sys.path.insert(0, str(TOOLS_ROOT))

from rating.rating import summarize


def load(path: str):
    records = [json.loads(line) for line in Path(path).read_text(encoding="utf-8").splitlines() if line.strip()]
    games = [r for r in records if r.get("type") == "game"]
    summary = next((r for r in records if r.get("type") == "summary"), None)
    return records, games, summary


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("jsonl")
    ap.add_argument("--require-games", type=int)
    ap.add_argument("--require-alternating", action="store_true")
    args = ap.parse_args()

    _, games, stored = load(args.jsonl)
    if not games:
        raise SystemExit("no game records found")

    aborted = [g for g in games if g["engine_a_result"] == "abort"]
    if aborted:
        raise SystemExit(f"benchmark contains {len(aborted)} aborted game(s)")

    wins = sum(g["engine_a_result"] == "win" for g in games)
    draws = sum(g["engine_a_result"] == "draw" for g in games)
    losses = sum(g["engine_a_result"] == "loss" for g in games)

    if args.require_games is not None and len(games) != args.require_games:
        raise SystemExit(f"expected {args.require_games} games, found {len(games)}")

    colors = [bool(g["engine_a_white"]) for g in games]
    white = sum(colors)
    black = len(colors) - white
    if args.require_alternating:
        for i, value in enumerate(colors):
            expected = (i % 2 == 0)
            if value != expected:
                raise SystemExit(f"color schedule mismatch at game {i + 1}")

    s = summarize(wins, draws, losses)
    print(f"Games: {s.games}")
    print(f"Engine A W/D/L: {s.wins}/{s.draws}/{s.losses}")
    print(f"Engine A score: {s.score:.4f} ({100*s.score:.2f}%)")
    print(f"Engine A-B performance Elo: {s.elo_delta:.1f}" if s.elo_delta is not None else "Engine A-B performance Elo: undefined")
    print(f"Approx score 95% CI: {s.score_ci95[0]:.4f} .. {s.score_ci95[1]:.4f}")
    if s.elo_ci95 is not None:
        print(f"Approx Elo 95% CI: {s.elo_ci95[0]:.1f} .. {s.elo_ci95[1]:.1f}")
    print(f"Colors A: White {white}, Black {black}")
    if stored:
        print(f"Stored summary games_completed: {stored.get('games_completed')}")
        if stored.get("games_completed") != s.games:
            raise SystemExit("stored summary disagrees with game records")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
