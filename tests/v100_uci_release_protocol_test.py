import os
import selectors
import subprocess
import time

exe = os.environ.get("CHESSZERO_BIN") or os.path.join(os.path.dirname(__file__), "..", "build", "chesszero")
proc = subprocess.Popen([exe], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                        stderr=subprocess.PIPE, bufsize=0)
sel = selectors.DefaultSelector()
sel.register(proc.stdout, selectors.EVENT_READ)
buf = b""

def send(line):
    proc.stdin.write((line + "\n").encode())
    proc.stdin.flush()

def read_some(timeout):
    deadline = time.time() + timeout
    while time.time() < deadline:
        events = sel.select(max(0.01, deadline - time.time()))
        for key, _ in events:
            chunk = os.read(key.fileobj.fileno(), 65536)
            if chunk:
                return chunk
            return b""
    return b""

def read_until(token, timeout=8.0):
    global buf
    wanted = token.encode()
    deadline = time.time() + timeout
    while time.time() < deadline:
        idx = buf.find(wanted)
        if idx >= 0:
            return buf[:idx + len(wanted)].decode(errors="replace")
        buf += read_some(max(0.01, deadline - time.time()))
    raise AssertionError(f"timeout waiting for {token!r}; output={buf.decode(errors='replace')!r}")

def read_until_line_containing(token, timeout=8.0):
    global buf
    wanted = token.encode()
    deadline = time.time() + timeout
    while time.time() < deadline:
        idx = buf.find(wanted)
        if idx >= 0:
            end = buf.find(b"\n", idx)
            if end >= 0:
                return buf[idx:end + 1].decode(errors="replace")
        buf += read_some(max(0.01, deadline - time.time()))
    raise AssertionError(f"timeout waiting for line containing {token!r}; output={buf.decode(errors='replace')!r}")

try:
    send("uci")
    out = read_until("uciok")
    assert "id name ChessZero v1." in out
    assert "option name Threads" in out
    assert "option name Move Overhead" in out
    assert "option name Slow Mover" in out

    send("isready")
    ready = read_until("readyok")
    assert "nnue=" in ready

    send("position startpos")
    send("book key")
    key_line = read_until_line_containing("polyglot key=")
    assert "polyglot key=463b96181691fc9c" in key_line.lower()

    # Exercise asynchronous stop and verify the next command still works.
    send("go depth 64")
    time.sleep(0.08)
    send("stop")
    best_line = read_until_line_containing("bestmove ", timeout=8.0)
    move = best_line.strip().split()[-1]
    assert len(move) >= 4

    send("setoption name Move Overhead value 50")
    send("setoption name Slow Mover value 120")
    send("isready")
    read_until("readyok")

    send("position startpos")
    send("go wtime 2500 btime 2500 winc 25 binc 25 movestogo 20")
    time.sleep(0.08)
    send("stop")
    read_until_line_containing("bestmove ", timeout=8.0)

    send("ucinewgame")
    read_until("newgame=cleared")
    send("quit")
    proc.wait(timeout=5)
    assert proc.returncode == 0, proc.returncode
    print("v1.0 UCI release protocol: PASS")
finally:
    if proc.poll() is None:
        proc.kill()
