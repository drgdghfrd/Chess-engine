#!/usr/bin/env python3
"""Train a multi-layer HalfKP network (HalfKAv2-style) for ChessZero.
Architecture: L1=256/persp (int16 accumulator, clip [0,127]) -> concat stm-first
(512 int8) -> L2=8 (clip) -> L3=32 (clip) -> scalar output.
Writes CZNNUE64 format consumed by NNUE::loadMemory (arch_=Multi).
Same CZDSV001 dataset input as train_halfkp.py.
"""
from __future__ import annotations
import argparse, os, struct
import numpy as np
MAGIC = b"CZDSV001"
NET_MAGIC = b"CZNNUE64"
INPUTS = 40960
HIDDEN = 256
L2 = 8
L3 = 32
SHIFT = 8
MAX_FEATURES = 60
def load_dataset(path):
    with open(path, "rb") as f:
        hdr = f.read(20)
        magic, version, max_features, count = struct.unpack("<8sIII", hdr)
        assert magic == MAGIC and version == 1 and max_features == MAX_FEATURES
        feats, stms, targets = [], [], []
        for i in range(count):
            n, stm, target = struct.unpack("<BBh", f.read(4))
            ids = np.frombuffer(f.read(2 * n), dtype="<u2").astype(np.int32, copy=True)
            feats.append(ids); stms.append(stm); targets.append(target / 32767.0)
            pad = -(4 + 2 * n) % 4
            if pad: f.read(pad)
    return feats, np.asarray(stms, dtype=np.int8), np.asarray(targets, dtype=np.float32)
def save_net(path, B, W, W2, B2, W3, B3, OW, OB):
    with open(path, "wb") as f:
        f.write(struct.pack("<8s6I", NET_MAGIC, 64, INPUTS, HIDDEN, 1, SHIFT, 0))
        np.asarray(B, dtype="<i2").tofile(f)          # L1 bias (shared) int16
        np.asarray(W, dtype="i1").tofile(f)           # L1 weights int8 [INPUTS,HIDDEN]
        np.asarray(W2, dtype="i1").tofile(f)          # L2 weights int8 [L2,2*HIDDEN]
        np.asarray(B2, dtype="<i4").tofile(f)         # L2 bias int32
        np.asarray(W3, dtype="i1").tofile(f)          # L3 weights int8 [L3,L2]
        np.asarray(B3, dtype="<i4").tofile(f)         # L3 bias int32
        np.asarray(OW, dtype="i1").tofile(f)          # output weights int8 [L3]
        f.write(struct.pack("<i", int(OB)))           # output bias int32
def forward(ids, stm, W, B, W2, B2, W3, B3, OW, OB):
    half = len(ids) // 2
    w_ids = ids[:half]; b_ids = ids[half:]
    a_w = B + W[w_ids].sum(axis=0)   # white-perspective accumulator
    a_b = B + W[b_ids].sum(axis=0)   # black-perspective accumulator
    stm_a, oth_a = (a_w, a_b) if stm else (a_b, a_w)
    l1 = np.clip(np.concatenate([stm_a, oth_a]), 0, 127)          # (512,)
    l2_pre = W2 @ l1 + B2                                          # (8,)
    l2 = np.clip(l2_pre, 0, 127)                                   # (8,)
    l3_pre = W3 @ l2 + B3                                          # (32,)
    l3 = np.clip(l3_pre, 0, 127)                                   # (32,)
    pred = float((np.dot(l3, OW) + OB) / 256.0)
    return pred, (a_w, a_b, l1, l2_pre, l2, l3_pre, l3)
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dataset"); ap.add_argument("output")
    ap.add_argument("--epochs", type=int, default=10)
    ap.add_argument("--lr-first", type=float, default=0.05)
    ap.add_argument("--lr-output", type=float, default=0.15)
    ap.add_argument("--target-scale", type=float, default=1500.0)
    ap.add_argument("--validation", type=float, default=0.10)
    ap.add_argument("--seed", type=int, default=7501)
    ap.add_argument("--weight-decay", type=float, default=1e-5)
    args = ap.parse_args()
    feats, stms, y = load_dataset(args.dataset)
    rng = np.random.default_rng(args.seed)
    order = rng.permutation(len(y)); nval = int(round(len(y) * args.validation))
    val_idx, train_idx = order[:nval], order[nval:]
    target = y * args.target_scale
    # Float master params
    W  = rng.normal(0, 0.8, (INPUTS, HIDDEN)).astype(np.float32)
    B  = np.zeros(HIDDEN, dtype=np.float32)
    W2 = rng.normal(0, 0.5, (L2, 2 * HIDDEN)).astype(np.float32) * 0.1
    B2 = np.zeros(L2, dtype=np.float32)
    W3 = rng.normal(0, 0.5, (L3, L2)).astype(np.float32) * 0.1
    B3 = np.zeros(L3, dtype=np.float32)
    OW = rng.normal(0, 4.0, L3).astype(np.float32)
    OB = np.float32(y[train_idx].mean() * args.target_scale * 256.0)
    print(f"records={len(y)} train={len(train_idx)} val={len(val_idx)} arch=512->{L2}->{L3}->1")
    for epoch in range(args.epochs):
        rng.shuffle(train_idx); loss = 0.0
        lr1 = args.lr_first * (0.7 ** epoch)
        lr2 = args.lr_output * (0.7 ** epoch)
        for i in train_idx:
            pred, cache = forward(feats[i], stms[i], W, B, W2, B2, W3, B3, OW, OB)
            a_w, a_b, l1, l2_pre, l2, l3_pre, l3 = cache
            err = float(np.clip(pred - target[i], -4000, 4000)); loss += err * err
            d = err / 256.0  # scalar gradient at output

            # Compute ALL gradients from the pre-update parameters. Updating W3/OW
            # before g_l2 (or W2 before g_l1) changes the gradient within the same
            # sample and makes this no longer standard backpropagation.
            act3 = (l3_pre > 0) & (l3_pre < 127)
            g_l3 = (d * OW) * act3                                   # (32,)
            g_OW = d * l3
            g_OB = d
            act2 = (l2_pre > 0) & (l2_pre < 127)
            g_l2 = (W3.T @ g_l3) * act2                              # (8,)
            g_W3 = np.outer(g_l3, l2); g_B3 = g_l3
            g_W2 = np.outer(g_l2, l1); g_B2 = g_l2
            g_l1 = W2.T @ g_l2                                        # (512,)
            half = len(feats[i]) // 2
            w_ids = feats[i][:half]; b_ids = feats[i][half:]
            stm_w = bool(stms[i])
            g_stm = g_l1[:HIDDEN]; g_oth = g_l1[HIDDEN:]
            a_stm, a_oth = (a_w, a_b) if stm_w else (a_b, a_w)
            ids_stm = w_ids if stm_w else b_ids
            ids_oth = b_ids if stm_w else w_ids
            act_s = (a_stm > 0) & (a_stm < 127)
            act_o = (a_oth > 0) & (a_oth < 127)
            gh_s = g_stm * act_s; gh_o = g_oth * act_o

            # Apply all parameter updates after the full gradient graph is frozen.
            W3 -= lr2 * g_W3; B3 -= lr2 * g_B3
            OW -= lr2 * g_OW; OB -= lr2 * g_OB
            W2 -= lr2 * g_W2; B2 -= lr2 * g_B2
            W[ids_stm] -= lr1 * gh_s
            W[ids_oth] -= lr1 * gh_o
            B -= lr1 * (gh_s + gh_o)
        if args.weight_decay:
            W -= lr1 * args.weight_decay * W
        tr_mse = loss / len(train_idx)
        # validation
        vloss = 0.0
        for i in val_idx:
            pred, _ = forward(feats[i], stms[i], W, B, W2, B2, W3, B3, OW, OB)
            vloss += (pred - float(target[i])) ** 2
        va_mse = vloss / max(1, len(val_idx)) if len(val_idx) else float("nan")
        print(f"epoch {epoch+1}/{args.epochs} train_mse={tr_mse:.0f} val_mse={va_mse:.0f} lr1={lr1:.4f}")
    # Quantize
    qB  = np.clip(np.rint(B), -32768, 32767).astype(np.int16)
    qW  = np.clip(np.rint(W), -127, 127).astype(np.int8)
    qW2 = np.clip(np.rint(W2), -127, 127).astype(np.int8)
    qB2 = np.clip(np.rint(B2), -2**31, 2**31-1).astype(np.int32)
    qW3 = np.clip(np.rint(W3), -127, 127).astype(np.int8)
    qB3 = np.clip(np.rint(B3), -2**31, 2**31-1).astype(np.int32)
    qOW = np.clip(np.rint(OW), -127, 127).astype(np.int8)
    qOB = int(np.clip(np.rint(OB), -2**31, 2**31-1))
    save_net(args.output, qB, qW, qW2, qB2, qW3, qB3, qOW, qOB)
    print(f"wrote {args.output} ({os.path.getsize(args.output)/1048576:.2f} MiB)")
if __name__ == "__main__":
    main()
