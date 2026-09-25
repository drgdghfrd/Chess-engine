#!/usr/bin/env python3
import os
import subprocess
import sys
import time

EXE = sys.argv[1] if len(sys.argv) > 1 else os.environ.get("CHESSZERO_BIN", "./chesszero")
proc = subprocess.Popen(
    [EXE], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
    stderr=subprocess.STDOUT, text=True, bufsize=1
)

def send(cmd):
    proc.stdin.write(cmd + "\n")
    proc.stdin.flush()


def read_until(pred, timeout=12.0):
    deadline = time.time() + timeout
    lines = []
    while time.time() < deadline:
        line = proc.stdout.readline()
        if line:
            lines.append(line.rstrip())
            if pred(line):
                return lines
        elif proc.poll() is not None:
            break
    raise AssertionError("timeout; output=" + repr(lines[-30:]))

try:
    send("uci")
    read_until(lambda s: s.strip() == "uciok")
    send("setoption name UseBook value false")
    send("position startpos")

    # Exercise every supported Chessis thread value while a search is active.
    for threads in range(1, 8):
        send("go infinite")
        time.sleep(0.12)
        lines = read_until(lambda s: "runtime_reconfigured" in s) if False else None
        # The setoption line itself triggers the transactional stop-and-apply.
        send(f"setoption name Threads value {threads}")
        lines = read_until(lambda s: "runtime_reconfigured" in s)
        assert not any(x.startswith("bestmove ") for x in lines)
        assert any(f"threads={threads}" in x and "parallel_workers=" in x for x in lines)
        send("isready")
        ready_lines = read_until(lambda s: s.strip() == "readyok")
        runtime = [x for x in ready_lines if "runtime_threads=" in x]
        assert runtime and f"runtime_threads={threads}" in runtime[-1]

    # Exercise all RAM profiles while the engine is active.
    for hash_mb in (16, 32, 64, 128, 256):
        send("go infinite")
        time.sleep(0.12)
        send(f"setoption name Hash value {hash_mb}")
        lines = read_until(lambda s: "runtime_reconfigured" in s)
        assert not any(x.startswith("bestmove ") for x in lines)
        assert any(
            f"hash_mb={hash_mb}" in x and
            f"allocated_hash_mb={hash_mb}" in x
            for x in lines
        )
        send("isready")
        ready_lines = read_until(lambda s: s.strip() == "readyok")
        runtime = [x for x in ready_lines if "runtime_hash_mb=" in x]
        assert runtime and f"runtime_hash_mb={hash_mb}" in runtime[-1]

    # Cross-change both dimensions in one sequence, mirroring GUI users who
    # change CPU and RAM settings repeatedly.
    sequence = [
        (1, 16), (2, 32), (3, 64), (4, 128),
        (5, 256), (6, 128), (7, 64), (5, 32), (2, 16), (7, 256)
    ]
    for threads, hash_mb in sequence:
        send("go infinite")
        time.sleep(0.12)
        send(f"setoption name Threads value {threads}")
        lines = read_until(lambda s: "runtime_reconfigured" in s)
        assert any(f"threads={threads}" in x for x in lines)
        send(f"setoption name Hash value {hash_mb}")
        lines = read_until(lambda s: "runtime_reconfigured" in s)
        assert any(
            f"threads={threads}" in x and
            f"hash_mb={hash_mb}" in x and
            f"allocated_hash_mb={hash_mb}" in x
            for x in lines
        )

    send("go depth 3")
    read_until(lambda s: s.startswith("bestmove "))
    send("quit")
    proc.wait(timeout=5)
    print("v1.1.0 UCI runtime reconfigure matrix: PASS")
except Exception:
    proc.kill()
    raise
