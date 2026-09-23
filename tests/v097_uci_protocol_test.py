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
            if wanted in buf:
                text = buf.decode(errors='replace')
                # Keep any bytes after the matched command for the next read.
                idx = buf.find(wanted) + len(wanted)
                return text[:idx]
    raise AssertionError(f'timeout waiting for {token!r}; output={buf.decode(errors="replace")!r}')

try:
    send('uci')
    out = read_until('uciok')
    assert 'id name ChessZero v' in out
    send('isready')
    read_until('readyok')
    send('position startpos')
    send('go depth 64')
    time.sleep(0.15)
    send('stop')
    out = read_until('bestmove', timeout=8.0)
    assert 'bestmove' in out
    send('ucinewgame')
    out = read_until('newgame=cleared')
    assert 'newgame=cleared' in out
    send('quit')
    proc.wait(timeout=5)
    assert proc.returncode == 0, proc.returncode
    print('v0.97 UCI asynchronous stop protocol: PASS')
finally:
    if proc.poll() is None:
        proc.kill()
