#!/usr/bin/env python3
"""Deterministic UCI benchmark for the packaged ChessZero HalfKP network.

The engine uses an asynchronous UCI worker, so each position must be searched to
`bestmove` before sending the next `position`/`go` command. Earlier benchmark code
queued multiple searches back-to-back and therefore measured cancellation rather
than completed search work.
"""
from __future__ import annotations

import argparse
import re
import subprocess
import time

FENS = [
    "rnbqkbnr/pppppppp/8/8/4P3/2N5/PPPP1PPP/R1BQKBNR b KQkq - 1 2",
    "8/8/8/8/8/4K3/4Q3/4k3 w - - 0 1",
]


class UCIError(RuntimeError):
    pass


def wait_prefix(proc: subprocess.Popen[str], prefix: str) -> str:
    while True:
        line = proc.stdout.readline()
        if not line:
            raise UCIError(f"engine exited while waiting for {prefix}")
        line = line.rstrip("\n")
        if line.startswith(prefix):
            return line


def search_position(proc: subprocess.Popen[str], fen: str, depth: int) -> tuple[int, int, str, float]:
    proc.stdin.write(f"position fen {fen}\n")
    proc.stdin.write(f"go depth {depth}\n")
    proc.stdin.flush()

    last_nodes = None
    last_depth = 0
    bestmove = ""
    t0 = time.perf_counter()
    while True:
        line = proc.stdout.readline()
        if not line:
            raise UCIError("engine exited during search")
        line = line.strip()
        m = re.match(r"info depth (\d+) .*?nodes (\d+)", line)
        if m:
            last_depth = int(m.group(1))
            last_nodes = int(m.group(2))
        if line.startswith("bestmove "):
            parts = line.split()
            bestmove = parts[1] if len(parts) > 1 else ""
            break
    elapsed_ms = int(round((time.perf_counter() - t0) * 1000))
    if last_nodes is None:
        raise UCIError("search completed without an info line containing nodes")
    return last_nodes, last_depth, bestmove, elapsed_ms


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("engine")
    ap.add_argument("network")
    ap.add_argument("--depth", type=int, default=2)
    args = ap.parse_args()

    proc = subprocess.Popen(
        [args.engine],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        text=True,
        bufsize=1,
    )
    try:
        proc.stdin.write("uci\n")
        proc.stdin.flush()
        wait_prefix(proc, "uciok")
        for command in (
            "setoption name UseBook value false",
            f"setoption name EvalFile value {args.network}",
            "isready",
        ):
            proc.stdin.write(command + "\n")
        proc.stdin.flush()
        wait_prefix(proc, "readyok")

        total_nodes = 0
        total_ms = 0
        for idx, fen in enumerate(FENS, 1):
            nodes, completed_depth, bestmove, elapsed_ms = search_position(proc, fen, args.depth)
            total_nodes += nodes
            total_ms += elapsed_ms
            print(
                f"position={idx} depth={completed_depth} nodes={nodes} "
                f"time_ms={elapsed_ms} bestmove={bestmove}"
            )

        nps = total_nodes * 1000 / max(total_ms, 1)
        print(
            f"positions={len(FENS)} requested_depth={args.depth} nodes={total_nodes} "
            f"time_ms={total_ms} nps={nps:.0f}"
        )
    finally:
        try:
            proc.stdin.write("quit\n")
            proc.stdin.flush()
            proc.wait(timeout=2)
        except Exception:
            proc.kill()


if __name__ == "__main__":
    try:
        main()
    except (OSError, UCIError) as exc:
        raise SystemExit(f"benchmark error: {exc}")
