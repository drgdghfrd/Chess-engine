#!/usr/bin/env python3
"""Elo-difference estimation from engine match results.

The confidence interval uses a Wilson interval on the half-point score
fraction. This is an intentionally conservative approximation for W/D/L data:
draws contribute 0.5 points, but the Wilson model itself is binomial.
It should therefore be reported as an approximate 95% interval, not as an
exact trinomial inference.
"""
from __future__ import annotations

import math
from dataclasses import dataclass
from typing import Optional, Tuple


@dataclass(frozen=True)
class RatingSummary:
    wins: int
    draws: int
    losses: int
    games: int
    score: float
    elo_delta: Optional[float]
    score_ci95: Tuple[float, float]
    elo_ci95: Optional[Tuple[float, float]]
    ci_method: str = "Wilson on half-point score (approximation)"

    def as_dict(self) -> dict:
        d = {
            "wins": self.wins,
            "draws": self.draws,
            "losses": self.losses,
            "games": self.games,
            "score": self.score,
            "elo_delta": self.elo_delta,
            "score_ci95": list(self.score_ci95),
            "elo_ci95": list(self.elo_ci95) if self.elo_ci95 is not None else None,
            "ci_method": self.ci_method,
        }
        return d


def _wilson_interval(x: float, n: int, z: float = 1.959963984540054) -> Tuple[float, float]:
    if n <= 0:
        return (0.0, 1.0)
    p = x / n
    denom = 1.0 + z * z / n
    center = (p + z * z / (2.0 * n)) / denom
    radius = z * math.sqrt((p * (1.0 - p) + z * z / (4.0 * n)) / n) / denom
    return (max(0.0, center - radius), min(1.0, center + radius))


def elo_from_score(score: float) -> Optional[float]:
    """Return Elo difference for a score fraction in (0, 1)."""
    if not 0.0 < score < 1.0:
        return None
    return -400.0 * math.log10(1.0 / score - 1.0)


def summarize(wins: int, draws: int, losses: int) -> RatingSummary:
    wins = int(wins)
    draws = int(draws)
    losses = int(losses)
    if min(wins, draws, losses) < 0:
        raise ValueError("W/D/L cannot be negative")
    games = wins + draws + losses
    if games == 0:
        raise ValueError("at least one game is required")

    score = (wins + 0.5 * draws) / games

    # Wilson is applied to the observed half-point score fraction as an
    # approximation. It remains useful for reporting precision consistently.
    lo, hi = _wilson_interval(score * games, games)
    elo = elo_from_score(score)

    if elo is None:
        elo_ci = None
    else:
        elo_lo = elo_from_score(lo)
        elo_hi = elo_from_score(hi)
        # Elo is monotone in score, so the interval endpoints remain ordered.
        elo_ci = (elo_lo, elo_hi) if elo_lo is not None and elo_hi is not None else None

    return RatingSummary(
        wins=wins,
        draws=draws,
        losses=losses,
        games=games,
        score=score,
        elo_delta=elo,
        score_ci95=(lo, hi),
        elo_ci95=elo_ci,
    )


def main() -> int:
    import argparse

    ap = argparse.ArgumentParser(description="Calculate engine Elo difference from W/D/L")
    ap.add_argument("--wins", type=int, required=True)
    ap.add_argument("--draws", type=int, required=True)
    ap.add_argument("--losses", type=int, required=True)
    args = ap.parse_args()
    print(summarize(args.wins, args.draws, args.losses).as_dict())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
