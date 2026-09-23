import os
import selectors
import subprocess
import time

exe = os.environ.get('CHESSZERO_BIN') or os.path.join(os.path.dirname(__file__), '..', 'build', 'chesszero')
proc = subprocess.Popen([exe], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                        stderr=subprocess.PIPE, bufsize=0)
sel = selectors.DefaultSelector()
sel.register(proc.stdout, selectors.EVENT_READ)
buf = b''

def send(s):
    proc.stdin.write((s + '\n').encode())
    proc.stdin.flush()

def read_until(token, timeout=8.0):
    global buf
    wanted = token.encode()
    end = time.time() + timeout
    while time.time() < end:
        events = sel.select(max(0.01, end - time.time()))
        for key, _ in events:
            chunk = os.read(key.fileobj.fileno(), 65536)
            if not chunk:
                break
            buf += chunk
            idx = buf.find(wanted)
            if idx >= 0:
                endidx = idx + len(wanted)
                text = buf.decode(errors='replace')
                return text[:endidx]
    raise AssertionError(f'timeout waiting for {token!r}; output={buf.decode(errors="replace")!r}')

try:
    send('uci')
    out = read_until('uciok')
    assert 'id name ChessZero v' in out

    send('isready')
    read_until('readyok')

    # Malformed/illegal moves must be reported and ignored, not mutate state.
    send('position startpos moves e2e4 a1a1 z9z9')
    out = read_until('position invalid_move=z9z9 ignored')
    assert 'position invalid_move=a1a1 ignored' in out

    # A normal search can be stopped and the protocol remains usable.
    send('go depth 64')
    time.sleep(0.08)
    send('stop')
    read_until('bestmove', timeout=8.0)

    send('isready')
    read_until('readyok')

    # Repeated go/stop after the previous search must not deadlock.
    send('position startpos')
    send('go wtime 2500 btime 2500 winc 25 binc 25 movestogo 20')
    time.sleep(0.08)
    send('stop')
    read_until('bestmove', timeout=8.0)

    send('ucinewgame')
    read_until('newgame=cleared')
    send('quit')
    proc.wait(timeout=5)
    assert proc.returncode == 0, proc.returncode
    print('v0.99 UCI RC protocol: PASS')
finally:
    if proc.poll() is None:
        proc.kill()
