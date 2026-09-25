#!/usr/bin/env python3
import os
import subprocess
import sys
import time

exe = sys.argv[1] if len(sys.argv) > 1 else os.environ.get("CHESSZERO_BIN", "./chesszero")
proc = subprocess.Popen([exe], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                        stderr=subprocess.STDOUT, text=True, bufsize=1)

def send(cmd):
    proc.stdin.write(cmd + "\n")
    proc.stdin.flush()

def read_until(pred, timeout=10.0):
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
    raise AssertionError("timeout; output=" + repr(lines[-20:]))

try:
    send("uci")
    read_until(lambda s: s.strip() == "uciok")
    send("setoption name Threads value 7")
    lines = read_until(lambda s: "runtime_reconfigured" in s)
    assert any("threads=7" in x and "hash_mb=32" in x for x in lines)

    send("position startpos")
    send("go infinite")
    time.sleep(0.15)
    send("setoption name Threads value 5")
    lines = read_until(lambda s: "runtime_reconfigured" in s)
    assert not any(x.startswith("bestmove ") for x in lines)
    assert any("threads=5" in x and "parallel_workers=4" in x for x in lines)

    send("setoption name Hash value 64")
    lines = read_until(lambda s: "runtime_reconfigured" in s)
    assert any("threads=5" in x and "hash_mb=64" in x and "allocated_hash_mb=64" in x for x in lines)

    send("isready")
    lines = read_until(lambda s: s.strip() == "readyok")
    runtime = [x for x in lines if "runtime_threads=" in x]
    assert runtime and "runtime_threads=5" in runtime[-1] and "runtime_hash_mb=64" in runtime[-1]

    send("setoption name Threads value 7")
    read_until(lambda s: "runtime_reconfigured" in s)
    send("setoption name Hash value 16")
    lines = read_until(lambda s: "runtime_reconfigured" in s)
    assert any("threads=7" in x and "hash_mb=16" in x and "allocated_hash_mb=16" in x for x in lines)

    send("quit")
    proc.wait(timeout=3)
    print("v1.1.0 UCI runtime reconfiguration test: PASS")
except Exception:
    proc.kill()
    raise
