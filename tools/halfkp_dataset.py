#!/usr/bin/env python3
"""Convert FEN + target text into the ChessZero HalfKP sparse dataset.

Input format (one position per line):
    <FEN>\t<target>

Target is normally in [-1, 1] from the side-to-move perspective.  A target
of 1 means a win, -1 a loss, and 0 a draw.  Comments beginning with # are
ignored.  A third tab-separated field is accepted as an optional game id.

Output format (little-endian):
    8 bytes magic: CZDSV001
    uint32 version = 1
    uint32 max_features = 60
    uint32 record_count
    records:
      uint8 feature_count
      uint8 side_to_move (0=black, 1=white)
      int16 target_q15
      uint16 feature_count feature indices (both perspectives, white first)
      padding to 4-byte record alignment
"""
from __future__ import annotations
import argparse, struct, sys

MAGIC = b"CZDSV001"
VERSION = 1
MAX_FEATURES = 60
INPUTS = 40960

PIECE_TYPES = {"P": 1, "N": 2, "B": 3, "R": 4, "Q": 5, "K": 6,
               "p": -1, "n": -2, "b": -3, "r": -4, "q": -5, "k": -6}


def parse_fen_board(fen: str):
    fields = fen.split()
    if len(fields) < 2:
        raise ValueError("FEN needs at least board and side-to-move fields")
    board, turn = fields[0], fields[1]
    if turn not in ("w", "b"):
        raise ValueError("invalid side-to-move")
    squares = [0] * 64
    rank = 7
    file_ = 0
    kings = {1: -1, -1: -1}
    for ch in board:
        if ch == "/":
            if file_ != 8 or rank == 0:
                raise ValueError("invalid FEN rank")
            rank -= 1
            file_ = 0
            continue
        if ch.isdigit():
            file_ += int(ch)
        elif ch in PIECE_TYPES:
            if file_ >= 8 or rank < 0:
                raise ValueError("invalid FEN board")
            p = PIECE_TYPES[ch]
            sq = rank * 8 + file_
            squares[sq] = p
            if abs(p) == 6:
                if kings[1 if p > 0 else -1] != -1:
                    raise ValueError("duplicate king")
                kings[1 if p > 0 else -1] = sq
            file_ += 1
        else:
            raise ValueError(f"invalid FEN piece: {ch}")
    if rank != 0 or file_ != 8:
        raise ValueError("invalid FEN board dimensions")
    if kings[1] < 0 or kings[-1] < 0:
        raise ValueError("both kings are required")
    return squares, (1 if turn == "w" else -1), kings


def feature_index(own_king_sq: int, piece: int, sq: int, white_perspective: bool) -> int:
    if piece == 0:
        return -1
    typ = abs(piece) - 1
    if typ == 5:  # king is the HalfKP anchor, not a feature
        return -1
    own = piece > 0 if white_perspective else piece < 0
    color_plane = 0 if own else 1
    piece_idx = color_plane * 5 + typ
    ksq = own_king_sq if white_perspective else (own_king_sq ^ 56)
    psq = sq if white_perspective else (sq ^ 56)
    idx = (ksq * 64 + psq) * 10 + piece_idx
    if not 0 <= idx < INPUTS:
        raise ValueError("feature index out of range")
    return idx


def encode_features(squares, kings):
    out = []
    for perspective in (True, False):
        ksq = kings[1 if perspective else -1]
        for sq, piece in enumerate(squares):
            idx = feature_index(ksq, piece, sq, perspective)
            if idx >= 0:
                out.append(idx)
    if len(out) > MAX_FEATURES:
        raise ValueError(f"too many HalfKP features: {len(out)}")
    return out


def target_q15(value: float) -> int:
    if not -1.000001 <= value <= 1.000001:
        raise ValueError("target must be in [-1, 1]")
    return max(-32768, min(32767, int(round(value * 32767.0))))


def parse_line(line: str):
    raw = line.strip()
    if not raw or raw.startswith("#"):
        return None
    parts = raw.split("\t")
    if len(parts) < 2:
        parts = raw.rsplit(None, 1)
    if len(parts) < 2:
        raise ValueError("expected '<FEN>\\t<target>'")
    fen, target_text = parts[0].strip(), parts[1].strip()
    target = float(target_text)
    squares, stm, kings = parse_fen_board(fen)
    features = encode_features(squares, kings)
    return features, stm, target_q15(target)


def write_dataset(records, out_path: str):
    with open(out_path, "wb") as f:
        f.write(struct.pack("<8sIII", MAGIC, VERSION, MAX_FEATURES, len(records)))
        for features, stm, target in records:
            f.write(struct.pack("<BBh", len(features), 1 if stm > 0 else 0, target))
            f.write(struct.pack("<%dH" % len(features), *features))
            used = 4 + 2 * len(features)
            pad = (-used) % 4
            if pad:
                f.write(b"\0" * pad)


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("input", help="FEN/target text file")
    ap.add_argument("output", help=".czds output file")
    args = ap.parse_args()
    records = []
    errors = 0
    with open(args.input, "r", encoding="utf-8") as f:
        for lineno, line in enumerate(f, 1):
            try:
                rec = parse_line(line)
                if rec is not None:
                    records.append(rec)
            except Exception as exc:
                errors += 1
                print(f"error:{lineno}: {exc}", file=sys.stderr)
    if errors:
        raise SystemExit(f"conversion failed: {errors} invalid line(s)")
    if not records:
        raise SystemExit("no training positions found")
    write_dataset(records, args.output)
    print(f"HalfKP dataset: {len(records)} positions -> {args.output}")
    print(f"features/position: {sum(len(r[0]) for r in records)/len(records):.1f} average")

if __name__ == "__main__":
    main()
