#!/usr/bin/env python3
"""Cross-product UCI benchmark for Threads × Hash profiles.

Designed for pre-Android regression checks. It records wall time, nodes,
NPS and best-move stability for every requested profile.
"""
from __future__ import annotations
import argparse
import csv
import statistics
import subprocess
import time


def run_once(engine: str, threads: int, hash_mb: int, depth: int) -> tuple[int, int, float, str]:
    p = subprocess.Popen([engine], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                         stderr=subprocess.PIPE, text=True, bufsize=1)
    assert p.stdin and p.stdout

    def send(cmd: str) -> None:
        p.stdin.write(cmd + "\n")
        p.stdin.flush()

    send("uci")
    while True:
        line = p.stdout.readline()
        if not line:
            raise RuntimeError("engine exited during uci")
        if line.strip() == "uciok":
            break
    send(f"setoption name Threads value {threads}")
    send(f"setoption name Hash value {hash_mb}")
    send("isready")
    while True:
        line = p.stdout.readline()
        if not line:
            raise RuntimeError("engine exited before readyok")
        if line.strip() == "readyok":
            break
    send("position startpos")
    t0 = time.perf_counter()
    send(f"go depth {depth}")
    nodes = 0
    done_depth = 0
    bestmove = ""
    while True:
        line = p.stdout.readline()
        if not line:
            raise RuntimeError("engine exited before bestmove")
        if line.startswith("info "):
            parts = line.split()
            for i, token in enumerate(parts[:-1]):
                if token == "nodes":
                    try:
                        nodes = int(parts[i + 1])
                    except ValueError:
                        pass
                elif token == "depth":
                    try:
                        done_depth = int(parts[i + 1])
                    except ValueError:
                        pass
        elif line.startswith("bestmove "):
            bestmove = line.split()[1]
            break
    ms = (time.perf_counter() - t0) * 1000.0
    send("quit")
    p.wait(timeout=5)
    return done_depth, nodes, ms, bestmove


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--engine", default="./build/chesszero")
    ap.add_argument("--threads", default="1,2,4,5,7")
    ap.add_argument("--hash", dest="hash_mb", default="16,32,64,128,256")
    ap.add_argument("--depth", type=int, default=8)
    ap.add_argument("--repeats", type=int, default=3)
    args = ap.parse_args()

    out = csv.writer(__import__("sys").stdout)
    out.writerow(["threads", "hash_mb", "depth", "nodes_median", "wall_ms_median",
                  "nps_median", "bestmove_set"])
    for threads in (int(x) for x in args.threads.split(",") if x.strip()):
        for hash_mb in (int(x) for x in args.hash_mb.split(",") if x.strip()):
            rows = [run_once(args.engine, threads, hash_mb, args.depth)
                    for _ in range(args.repeats)]
            med_ms = statistics.median(r[2] for r in rows)
            med_nodes = statistics.median(r[1] for r in rows)
            nps = med_nodes / (med_ms / 1000.0) if med_ms else 0
            out.writerow([threads, hash_mb, rows[-1][0], int(med_nodes),
                          round(med_ms, 3), int(nps),
                          ";".join(sorted({r[3] for r in rows}))])


if __name__ == "__main__":
    main()
