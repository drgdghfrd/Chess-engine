#!/usr/bin/env python3
"""Train the first ChessZero HalfKP 40960x256 network.

This v0.75 trainer is a reproducible sparse-SGD reference trainer.  It accepts
CZDSV001 datasets, makes a deterministic train/validation split, reports MSE,
and writes the exact CZNNUE32 format consumed by the C++ runtime.
"""
from __future__ import annotations
import argparse, os, struct
import numpy as np

MAGIC = b"CZDSV001"
NET_MAGIC = b"CZNNUE32"
INPUTS = 40960
HIDDEN = 256
SHIFT = 8
MAX_FEATURES = 60


def load_dataset(path):
    with open(path, "rb") as f:
        hdr = f.read(20)
        if len(hdr) != 20:
            raise ValueError("truncated dataset header")
        magic, version, max_features, count = struct.unpack("<8sIII", hdr)
        if magic != MAGIC or version != 1 or max_features != MAX_FEATURES:
            raise ValueError("unsupported dataset")
        feats, stms, targets = [], [], []
        for i in range(count):
            head = f.read(4)
            if len(head) != 4:
                raise ValueError(f"truncated record {i}")
            n, stm, target = struct.unpack("<BBh", head)
            if n > MAX_FEATURES:
                raise ValueError(f"record {i}: too many features")
            raw = f.read(2 * n)
            if len(raw) != 2 * n:
                raise ValueError(f"truncated record {i}")
            ids = np.frombuffer(raw, dtype="<u2").astype(np.int32, copy=True)
            feats.append(ids)
            stms.append(stm)
            targets.append(target / 32767.0)
            pad = -(4 + 2 * n) % 4
            if pad and len(f.read(pad)) != pad:
                raise ValueError(f"truncated padding at record {i}")
    if not feats:
        raise ValueError("empty dataset")
    return feats, np.asarray(stms, dtype=np.int8), np.asarray(targets, dtype=np.float32)


def save_net(path, bias, weights, out_w, out_bias):
    with open(path, "wb") as f:
        f.write(struct.pack("<8s6I", NET_MAGIC, 32, INPUTS, HIDDEN, 1, SHIFT, 0))
        np.asarray(bias, dtype="<i2").tofile(f)
        np.asarray(weights, dtype="i1").tofile(f)
        np.asarray(out_w, dtype="i1").tofile(f)
        f.write(struct.pack("<i", int(out_bias)))


def predict(ids, stm, W, B, OW, OB, scale):
    # ids are stored as white-perspective features first and black-perspective
    # features second. Both halves contain the same number of non-king pieces.
    n = len(ids)
    half = n // 2
    active_ids = ids[:half] if stm else ids[half:]
    a = B + W[active_ids].sum(axis=0)
    clipped = np.clip(a, 0, 127)
    return float((np.dot(clipped, OW) + OB) / 256.0), a, clipped, active_ids


def evaluate_split(indices, feats, stms, y, W, B, OW, OB, scale):
    se = 0.0
    for i in indices:
        pred, _, _, _ = predict(feats[i], stms[i], W, B, OW, OB, scale)
        se += (pred - float(y[i] * scale)) ** 2
    return se / max(1, len(indices))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("dataset")
    ap.add_argument("output")
    ap.add_argument("--epochs", type=int, default=5)
    ap.add_argument("--lr-first", type=float, default=0.08)
    ap.add_argument("--lr-output", type=float, default=0.20)
    ap.add_argument("--target-scale", type=float, default=1000.0)
    ap.add_argument("--validation", type=float, default=0.10)
    ap.add_argument("--seed", type=int, default=7501)
    args = ap.parse_args()
    if args.epochs < 1 or not 0 <= args.validation < 1:
        raise SystemExit("invalid epochs/validation")

    feats, stms, y = load_dataset(args.dataset)
    rng = np.random.default_rng(args.seed)
    order = rng.permutation(len(y))
    nval = int(round(len(y) * args.validation))
    val_idx = order[:nval]
    train_idx = order[nval:]
    if not len(train_idx):
        raise SystemExit("validation split consumed the entire dataset")

    # Float master parameters. Small initialization keeps the first few
    # accumulators in the useful clipped-ReLU range.
    W = rng.normal(0.0, 0.8, size=(INPUTS, HIDDEN)).astype(np.float32)
    B = np.zeros(HIDDEN, dtype=np.float32)
    OW = rng.normal(0.0, 4.0, size=HIDDEN).astype(np.float32)
    OB = np.float32(y[train_idx].mean() * args.target_scale * 256.0)
    target = y * args.target_scale

    print(f"dataset={args.dataset} records={len(y)} train={len(train_idx)} val={len(val_idx)}")
    print(f"target_scale={args.target_scale:.1f} lr_first={args.lr_first:g} lr_output={args.lr_output:g}")

    for epoch in range(args.epochs):
        rng.shuffle(train_idx)
        loss = 0.0
        for i in train_idx:
            pred, a, clipped, ids = predict(feats[i], stms[i], W, B, OW, OB, args.target_scale)
            err = float(np.clip(pred - target[i], -4000.0, 4000.0))
            loss += err * err
            d = err / 256.0

            # Compute hidden gradient before changing OW. Bias is updated once
            # per sample, not once per active feature.
            active = (a > 0.0) & (a < 127.0)
            grad_h = (d * OW) * active
            W[ids] -= args.lr_first * grad_h
            B -= args.lr_first * grad_h
            OW -= args.lr_output * d * clipped
            OB -= args.lr_output * d

        train_mse = loss / len(train_idx)
        val_mse = evaluate_split(val_idx, feats, stms, y, W, B, OW, OB, args.target_scale) if len(val_idx) else float("nan")
        print(f"epoch {epoch + 1}/{args.epochs} train_mse={train_mse:.3f} val_mse={val_mse:.3f}")

    qB = np.clip(np.rint(B), -32768, 32767).astype(np.int16)
    qW = np.clip(np.rint(W), -127, 127).astype(np.int8)
    qOW = np.clip(np.rint(OW), -127, 127).astype(np.int8)
    qOB = int(np.clip(np.rint(OB), -2147483648, 2147483647))
    save_net(args.output, qB, qW, qOW, qOB)
    print(f"wrote {args.output} ({os.path.getsize(args.output) / 1048576:.2f} MiB)")
    print(f"quantized ranges: W=[{qW.min()},{qW.max()}] OW=[{qOW.min()},{qOW.max()}] OB={qOB}")

if __name__ == "__main__":
    main()
