#!/usr/bin/env python3
from pathlib import Path
import hashlib, struct

ROOT = Path(__file__).resolve().parents[1]
SINGLE = ROOT / "nets/ChessZero-v1.0.9-single_1m.nnue"
MULTI = ROOT / "nets/ChessZero-v1.0.9-multi_1m.nnue"
CURRENT = ROOT / "nets/ChessZero-v1.0.9-current.nnue"
ASSET = ROOT / "android/app/src/main/assets/ChessZero-v1.0.9-current.nnue"

def header(p):
    b = p.read_bytes()
    if len(b) < 32:
        raise AssertionError(f"{p}: too small")
    magic, version, inputs, hidden, output, shift, reserved = struct.unpack("<8s6I", b[:32])
    return b, magic, version, inputs, hidden, output, shift, reserved

# CZNNUE32 exact payload size: header + L1 bias + L1 weights + output weights + output bias.
expected_single = 32 + 256*2 + 40960*256 + 256 + 4
# CZNNUE64 exact payload size: header + L1 + L2 + L3 + output head.
expected_multi = 32 + 256*2 + 40960*256 + (2*256*8) + 8*4 + (8*32) + 32*4 + 32 + 4

bs, ms, vs, ins, hs, outs, sh, _ = header(SINGLE)
bm, mm, vm, inm, hm, outm, shm, _ = header(MULTI)
bc, mc, vc, inc, hc, outc, shc, _ = header(CURRENT)

assert len(bs) == expected_single, (len(bs), expected_single)
assert len(bm) == expected_multi, (len(bm), expected_multi)
assert len(bc) == expected_single, (len(bc), expected_single)
assert ms == b"CZNNUE32" and vs == 32 and ins == 40960 and hs == 256 and outs == 1 and sh == 8
assert mm == b"CZNNUE64" and vm == 64 and inm == 40960 and hm == 256 and outm == 1 and shm == 8
assert mc == ms and vc == vs and inc == ins and hc == hs and outc == outs and shc == sh
assert bc == (ROOT / "nets/ChessZero-v1.0.9-sf250k.nnue").read_bytes(), "current network must equal sf250k production baseline"
assert not ASSET.exists(), "raw NNUE asset must not be duplicated into APK; CMake embeds nets/current.nnue"

print("v1.11 1M NNUE + production baseline integrity: PASS")
print("single_sha256=" + hashlib.sha256(bs).hexdigest())
print("multi_sha256=" + hashlib.sha256(bm).hexdigest())
print(f"single_bytes={len(bs)} multi_bytes={len(bm)}")
