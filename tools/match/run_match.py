#!/usr/bin/env python3
"""UCI match runner for measuring one engine against a distinct opponent.

The match adjudication is performed locally so the opponent does not need any
non-standard UCI commands such as ChessZero's ``d``/``info string status=``.
"""
import argparse
import json
import os
import subprocess
import sys
from pathlib import Path

from elo import summarize

FILES = "abcdefgh"
PROMOTIONS = "qrbn"


class UCIError(RuntimeError):
    pass


class Engine:
    def __init__(self, path, mode=None, options=None, name=None):
        self.path = os.path.abspath(path)
        if not os.path.isfile(self.path):
            raise UCIError(f"engine executable not found: {self.path}")
        if not os.access(self.path, os.X_OK):
            raise UCIError(f"engine is not executable: {self.path}")
        self.name = name or Path(self.path).name
        self.uci_name = None
        self.uci_author = None
        self.p = subprocess.Popen(
            [self.path],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
            bufsize=1,
        )
        self.send("uci")
        while True:
            line = self.p.stdout.readline()
            if not line:
                raise UCIError(f"engine exited while waiting for uciok: {self.name}")
            line = line.strip()
            if line.startswith("id name "):
                self.uci_name = line[8:]
            elif line.startswith("id author "):
                self.uci_author = line[10:]
            if line == "uciok":
                break
        # Stable benchmark defaults. User-supplied options are applied last so
        # they can intentionally override a default (for example Hash).
        if mode:
            self.send(f"setoption name EvalMode value {mode}")
        self.send("setoption name UseBook value false")
        self.send("setoption name Hash value 16")
        for option in options or []:
            self.send(f"setoption name {option[0]} value {option[1]}")
        self.send("isready")
        self.wait_prefix("readyok")

    def send(self, command):
        if self.p.poll() is not None:
            raise UCIError(f"engine exited before command: {self.name}")
        try:
            self.p.stdin.write(command + "\n")
            self.p.stdin.flush()
        except (BrokenPipeError, OSError) as exc:
            raise UCIError(f"engine write failed: {self.name}: {exc}") from exc

    def wait_prefix(self, prefix):
        while True:
            line = self.p.stdout.readline()
            if not line:
                raise UCIError(f"engine exited while waiting for {prefix}: {self.name}")
            if line.startswith(prefix):
                return line.strip()

    @staticmethod
    def _position_command(moves):
        return "position startpos" + ((" moves " + " ".join(moves)) if moves else "")

    def bestmove(self, moves, depth=None, movetime=None):
        if depth is None and movetime is None:
            raise ValueError("one of depth or movetime is required")
        if depth is not None and movetime is not None:
            raise ValueError("depth and movetime are mutually exclusive")
        self.send(self._position_command(moves))
        self.send(f"go depth {depth}" if depth is not None else f"go movetime {movetime}")
        while True:
            line = self.p.stdout.readline()
            if not line:
                raise UCIError(f"engine exited during search: {self.name}")
            if line.startswith("bestmove "):
                parts = line.split()
                move = parts[1] if len(parts) > 1 else ""
                return "" if move in ("0000", "(none)") else move

    def new_game(self):
        self.send("ucinewgame")
        self.send("isready")
        self.wait_prefix("readyok")

    def close(self):
        if self.p.poll() is None:
            try:
                self.send("quit")
                self.p.wait(timeout=2)
            except Exception:
                self.p.kill()


class Board:
    """Small, dependency-free legal-move/adjudication board for UCI matches."""

    def __init__(self):
        self.start()

    def start(self):
        self.board = ["."] * 64
        placement = [
            "rnbqkbnr",
            "pppppppp",
            "........",
            "........",
            "........",
            "........",
            "PPPPPPPP",
            "RNBQKBNR",
        ]
        for rank, row in enumerate(placement):
            for file_idx, piece in enumerate(row):
                self.board[(7 - rank) * 8 + file_idx] = piece
        self.side = "w"
        self.castling = set("KQkq")
        self.ep = None
        self.halfmove = 0
        self.fullmove = 1
        self.repetition = {self.key(): 1}

    @staticmethod
    def sq(s):
        if len(s) != 2 or s[0] not in FILES or s[1] not in "12345678":
            raise ValueError(f"invalid square: {s}")
        return (int(s[1]) - 1) * 8 + FILES.index(s[0])

    @staticmethod
    def uci_square(sq):
        return FILES[sq & 7] + str((sq >> 3) + 1)

    @staticmethod
    def color(piece):
        if piece == ".":
            return None
        return "w" if piece.isupper() else "b"

    @staticmethod
    def opponent(side):
        return "b" if side == "w" else "w"

    def key(self):
        effective_ep = self.ep
        if effective_ep is not None:
            ep_file = effective_ep & 7
            ep_rank = effective_ep >> 3
            pawn = "P" if self.side == "w" else "p"
            from_rank = ep_rank - 1 if self.side == "w" else ep_rank + 1
            sources = []
            if 0 <= from_rank < 8:
                if ep_file > 0:
                    sources.append(from_rank * 8 + ep_file - 1)
                if ep_file < 7:
                    sources.append(from_rank * 8 + ep_file + 1)
            if not any(self.board[s] == pawn for s in sources):
                effective_ep = None
        return "".join(self.board) + self.side + "".join(sorted(self.castling)) + (
            self.uci_square(effective_ep) if effective_ep is not None else "-"
        )

    def attacked(self, square, by_side):
        rank, file_idx = square >> 3, square & 7
        pawn = "P" if by_side == "w" else "p"
        pawn_rank = rank - 1 if by_side == "w" else rank + 1
        if 0 <= pawn_rank < 8:
            for df in (-1, 1):
                ff = file_idx + df
                if 0 <= ff < 8 and self.board[pawn_rank * 8 + ff] == pawn:
                    return True
        knight = "N" if by_side == "w" else "n"
        for dr, df in ((2, 1), (2, -1), (-2, 1), (-2, -1),
                      (1, 2), (1, -2), (-1, 2), (-1, -2)):
            rr, ff = rank + dr, file_idx + df
            if 0 <= rr < 8 and 0 <= ff < 8 and self.board[rr * 8 + ff] == knight:
                return True
        king = "K" if by_side == "w" else "k"
        for dr, df in ((1, 0), (-1, 0), (0, 1), (0, -1),
                      (1, 1), (1, -1), (-1, 1), (-1, -1)):
            rr, ff = rank + dr, file_idx + df
            if 0 <= rr < 8 and 0 <= ff < 8 and self.board[rr * 8 + ff] == king:
                return True
        for dirs, pieces in (
            (((1, 0), (-1, 0), (0, 1), (0, -1)), "RQ"),
            (((1, 1), (1, -1), (-1, 1), (-1, -1)), "BQ"),
        ):
            for dr, df in dirs:
                rr, ff = rank + dr, file_idx + df
                while 0 <= rr < 8 and 0 <= ff < 8:
                    piece = self.board[rr * 8 + ff]
                    if piece != ".":
                        if self.color(piece) == by_side and piece.upper() in pieces:
                            return True
                        break
                    rr += dr
                    ff += df
        return False

    def in_check(self, side):
        king = "K" if side == "w" else "k"
        try:
            king_sq = self.board.index(king)
        except ValueError as exc:
            raise UCIError(f"position has no {side} king") from exc
        return self.attacked(king_sq, self.opponent(side))

    def pseudo(self, side):
        for sq, piece in enumerate(self.board):
            if self.color(piece) != side:
                continue
            rank, file_idx = sq >> 3, sq & 7
            kind = piece.upper()
            if kind == "P":
                direction = 1 if side == "w" else -1
                start_rank = 1 if side == "w" else 6
                promo_rank = 7 if side == "w" else 0
                one_rank = rank + direction
                if 0 <= one_rank < 8:
                    one = one_rank * 8 + file_idx
                    if self.board[one] == ".":
                        if one_rank == promo_rank:
                            for promo in PROMOTIONS:
                                yield (sq, one, promo)
                        else:
                            yield (sq, one, None)
                            if rank == start_rank:
                                two = (rank + 2 * direction) * 8 + file_idx
                                if self.board[two] == ".":
                                    yield (sq, two, None)
                    for df in (-1, 1):
                        ff = file_idx + df
                        if 0 <= ff < 8:
                            to = one_rank * 8 + ff
                            target = self.board[to]
                            if (target != "." and self.color(target) != side) or to == self.ep:
                                if one_rank == promo_rank:
                                    for promo in PROMOTIONS:
                                        yield (sq, to, promo)
                                else:
                                    yield (sq, to, None)
            elif kind == "N":
                for dr, df in ((2, 1), (2, -1), (-2, 1), (-2, -1),
                               (1, 2), (1, -2), (-1, 2), (-1, -2)):
                    rr, ff = rank + dr, file_idx + df
                    if 0 <= rr < 8 and 0 <= ff < 8:
                        to = rr * 8 + ff
                        if self.color(self.board[to]) != side:
                            yield (sq, to, None)
            elif kind in "BRQ":
                directions = []
                if kind in "RQ":
                    directions += [(1, 0), (-1, 0), (0, 1), (0, -1)]
                if kind in "BQ":
                    directions += [(1, 1), (1, -1), (-1, 1), (-1, -1)]
                for dr, df in directions:
                    rr, ff = rank + dr, file_idx + df
                    while 0 <= rr < 8 and 0 <= ff < 8:
                        to = rr * 8 + ff
                        if self.board[to] == ".":
                            yield (sq, to, None)
                        else:
                            if self.color(self.board[to]) != side:
                                yield (sq, to, None)
                            break
                        rr += dr
                        ff += df
            elif kind == "K":
                for dr, df in ((1, 0), (-1, 0), (0, 1), (0, -1),
                               (1, 1), (1, -1), (-1, 1), (-1, -1)):
                    rr, ff = rank + dr, file_idx + df
                    if 0 <= rr < 8 and 0 <= ff < 8:
                        to = rr * 8 + ff
                        if self.color(self.board[to]) != side:
                            yield (sq, to, None)
                if side == "w" and sq == self.sq("e1"):
                    if "K" in self.castling and self.board[self.sq("f1")] == "." and self.board[self.sq("g1")] == ".":
                        if not self.in_check("w") and not self.attacked(self.sq("f1"), "b") and not self.attacked(self.sq("g1"), "b"):
                            yield (sq, self.sq("g1"), None)
                    if "Q" in self.castling and self.board[self.sq("b1")] == "." and self.board[self.sq("c1")] == "." and self.board[self.sq("d1")] == ".":
                        if not self.in_check("w") and not self.attacked(self.sq("d1"), "b") and not self.attacked(self.sq("c1"), "b"):
                            yield (sq, self.sq("c1"), None)
                if side == "b" and sq == self.sq("e8"):
                    if "k" in self.castling and self.board[self.sq("f8")] == "." and self.board[self.sq("g8")] == ".":
                        if not self.in_check("b") and not self.attacked(self.sq("f8"), "w") and not self.attacked(self.sq("g8"), "w"):
                            yield (sq, self.sq("g8"), None)
                    if "q" in self.castling and self.board[self.sq("b8")] == "." and self.board[self.sq("c8")] == "." and self.board[self.sq("d8")] == ".":
                        if not self.in_check("b") and not self.attacked(self.sq("d8"), "w") and not self.attacked(self.sq("c8"), "w"):
                            yield (sq, self.sq("c8"), None)

    def _push_unchecked(self, move):
        frm, to, promo = move
        piece = self.board[frm]
        target = self.board[to]
        self.board[to] = piece
        self.board[frm] = "."
        if piece.upper() == "P" and to == self.ep and target == "." and (to & 7) != (frm & 7):
            captured = to - 8 if self.side == "w" else to + 8
            self.board[captured] = "."
        if promo:
            self.board[to] = promo.upper() if self.side == "w" else promo.lower()
        if piece == "K":
            self.castling.discard("K"); self.castling.discard("Q")
            if frm == self.sq("e1") and to == self.sq("g1"):
                self.board[self.sq("f1")] = self.board[self.sq("h1")]; self.board[self.sq("h1")] = "."
            elif frm == self.sq("e1") and to == self.sq("c1"):
                self.board[self.sq("d1")] = self.board[self.sq("a1")]; self.board[self.sq("a1")] = "."
        elif piece == "k":
            self.castling.discard("k"); self.castling.discard("q")
            if frm == self.sq("e8") and to == self.sq("g8"):
                self.board[self.sq("f8")] = self.board[self.sq("h8")]; self.board[self.sq("h8")] = "."
            elif frm == self.sq("e8") and to == self.sq("c8"):
                self.board[self.sq("d8")] = self.board[self.sq("a8")]; self.board[self.sq("a8")] = "."
        if piece == "R" and frm == self.sq("h1"): self.castling.discard("K")
        if piece == "R" and frm == self.sq("a1"): self.castling.discard("Q")
        if piece == "r" and frm == self.sq("h8"): self.castling.discard("k")
        if piece == "r" and frm == self.sq("a8"): self.castling.discard("q")
        if target == "R" and to == self.sq("h1"): self.castling.discard("K")
        if target == "R" and to == self.sq("a1"): self.castling.discard("Q")
        if target == "r" and to == self.sq("h8"): self.castling.discard("k")
        if target == "r" and to == self.sq("a8"): self.castling.discard("q")

        self.ep = None
        if piece.upper() == "P" and abs(to - frm) == 16:
            self.ep = (to + frm) // 2
        if piece.upper() == "P" or target != ".":
            self.halfmove = 0
        else:
            self.halfmove += 1
        if self.side == "b":
            self.fullmove += 1
        self.side = self.opponent(self.side)

    def legal_moves(self):
        side = self.side
        for move in list(self.pseudo(side)):
            snapshot = (
                self.board[:], self.side, set(self.castling), self.ep,
                self.halfmove, self.fullmove,
            )
            self._push_unchecked(move)
            legal = not self.in_check(side)
            self.board, self.side, self.castling, self.ep, self.halfmove, self.fullmove = snapshot
            if legal:
                yield move

    @staticmethod
    def move_to_uci(move):
        frm, to, promo = move
        return Board.uci_square(frm) + Board.uci_square(to) + (promo or "")

    def push_uci(self, uci):
        if len(uci) not in (4, 5):
            raise UCIError(f"illegal/malformed engine move: {uci}")
        try:
            frm, to = self.sq(uci[:2]), self.sq(uci[2:4])
        except ValueError as exc:
            raise UCIError(f"illegal/malformed engine move: {uci}") from exc
        promo = uci[4].lower() if len(uci) == 5 else None
        if promo not in (None,) + tuple(PROMOTIONS):
            raise UCIError(f"illegal/malformed engine move: {uci}")
        if self.board[to].upper() == "K":
            raise UCIError(f"illegal engine move {uci}: king capture")
        candidate = (frm, to, promo)
        legal = list(self.legal_moves())
        if candidate not in legal:
            raise UCIError(f"illegal engine move {uci} in current position")
        self._push_unchecked(candidate)
        self.repetition[self.key()] = self.repetition.get(self.key(), 0) + 1

    def insufficient_material(self):
        pieces = [p for p in self.board if p != "."]
        if any(p.upper() in "QR" for p in pieces):
            return False
        non_kings = [p for p in pieces if p.upper() != "K"]
        if not non_kings:
            return True
        if len(non_kings) == 1 and non_kings[0].upper() in "BN":
            return True
        if len(non_kings) == 2 and all(p.upper() == "B" for p in non_kings):
            squares = [sq for sq, p in enumerate(self.board) if p.upper() == "B"]
            return ((squares[0] >> 3) + (squares[0] & 7)) % 2 == ((squares[1] >> 3) + (squares[1] & 7)) % 2
        return False

    def result(self):
        legal = list(self.legal_moves())
        if not legal:
            if self.in_check(self.side):
                return "1-0" if self.side == "b" else "0-1"
            return "1/2-1/2"
        if self.halfmove >= 100 or self.repetition.get(self.key(), 0) >= 3 or self.insufficient_material():
            return "1/2-1/2"
        return None


def parse_options(items):
    options = []
    for item in items or []:
        if "=" not in item:
            raise ValueError(f"engine option must be NAME=VALUE: {item}")
        name, value = item.split("=", 1)
        name, value = name.strip(), value.strip()
        if not name:
            raise ValueError(f"engine option name is empty: {item}")
        options.append((name, value))
    return options


def game(white_path, white_mode, white_options, white_depth, white_movetime,
         black_path, black_mode, black_options, black_depth, black_movetime,
         max_plies):
    ew = Engine(white_path, white_mode, white_options, "white")
    eb = Engine(black_path, black_mode, black_options, "black")
    board = Board()
    moves = []
    result = None
    try:
        ew.new_game(); eb.new_game()
        for ply in range(max_plies):
            mover = ew if board.side == "w" else eb
            depth = white_depth if board.side == "w" else black_depth
            movetime = white_movetime if board.side == "w" else black_movetime
            mv = mover.bestmove(moves, depth, movetime)
            if not mv or mv == "(none)":
                if board.in_check(board.side):
                    result = "1-0" if board.side == "b" else "0-1"
                else:
                    result = "1/2-1/2"
                break
            board.push_uci(mv)
            moves.append(mv)
            result = board.result()
            if result is not None:
                break
        if result is None:
            result = "1/2-1/2"
        meta = {
            "white_uci_name": ew.uci_name, "white_uci_author": ew.uci_author,
            "black_uci_name": eb.uci_name, "black_uci_author": eb.uci_author,
        }
        return result, moves, meta
    finally:
        ew.close(); eb.close()


def main():
    ap = argparse.ArgumentParser(description="ChessZero v1.0.2 cross-engine UCI match runner")
    ap.add_argument("--engine-a", required=True)
    ap.add_argument("--engine-b", required=True)
    ap.add_argument("--mode-a", default=None)
    ap.add_argument("--mode-b", default=None)
    ap.add_argument("--option-a", action="append", default=[], metavar="NAME=VALUE",
                    help="extra UCI option for engine A; repeatable")
    ap.add_argument("--option-b", action="append", default=[], metavar="NAME=VALUE",
                    help="extra UCI option for engine B; repeatable")
    ap.add_argument("--depth", type=int, default=None, help="search depth for both engines")
    ap.add_argument("--depth-a", type=int, default=None)
    ap.add_argument("--depth-b", type=int, default=None)
    ap.add_argument("--movetime", type=int, default=None, help="milliseconds per move for both engines")
    ap.add_argument("--movetime-a", type=int, default=None)
    ap.add_argument("--movetime-b", type=int, default=None)
    ap.add_argument("--games", type=int, default=100)
    ap.add_argument("--max-plies", type=int, default=200)
    ap.add_argument("--rating-b", type=float, default=None,
                    help="optional published/reference rating for B; only used to offset A's measured delta")
    ap.add_argument("--out", "--output", default="match_result.json")
    ap.add_argument("--pgn", default=None, help="optional PGN output path")
    ap.add_argument("--jsonl", default=None, help="optional one-game-per-line JSONL output path")
    args = ap.parse_args()

    if args.depth is None and args.movetime is None and args.depth_a is None and args.depth_b is None and args.movetime_a is None and args.movetime_b is None:
        ap.error("specify --depth/--movetime (shared) or per-engine depth/movetime")
    if args.depth is not None and args.movetime is not None:
        ap.error("--depth and --movetime are mutually exclusive")
    for value, label in ((args.games, "games"), (args.max_plies, "max-plies")):
        if value <= 0:
            ap.error(f"--{label} must be > 0")

    depth_a = args.depth_a if args.depth_a is not None else args.depth
    depth_b = args.depth_b if args.depth_b is not None else args.depth
    mt_a = args.movetime_a if args.movetime_a is not None else args.movetime
    mt_b = args.movetime_b if args.movetime_b is not None else args.movetime
    if (depth_a is None) == (mt_a is None) or (depth_b is None) == (mt_b is None):
        ap.error("each engine must have exactly one of depth or movetime")
    if depth_a is not None and depth_a <= 0 or depth_b is not None and depth_b <= 0:
        ap.error("depth must be > 0")
    if mt_a is not None and mt_a <= 0 or mt_b is not None and mt_b <= 0:
        ap.error("movetime must be > 0")

    options_a = parse_options(args.option_a)
    options_b = parse_options(args.option_b)
    details = []
    w = d = l = 0
    identities = {
        "A": {"path": os.path.abspath(args.engine_a)},
        "B": {"path": os.path.abspath(args.engine_b)},
    }

    for i in range(args.games):
        a_is_white = (i % 2 == 0)
        if a_is_white:
            res, moves, meta = game(args.engine_a, args.mode_a, options_a, depth_a, mt_a,
                              args.engine_b, args.mode_b, options_b, depth_b, mt_b,
                              args.max_plies)
            if res == "1-0": w += 1
            elif res == "0-1": l += 1
            else: d += 1
        else:
            res, moves, meta = game(args.engine_b, args.mode_b, options_b, depth_b, mt_b,
                              args.engine_a, args.mode_a, options_a, depth_a, mt_a,
                              args.max_plies)
            if res == "0-1": w += 1
            elif res == "1-0": l += 1
            else: d += 1
        if a_is_white:
            identities["A"].update({"uci_name": meta["white_uci_name"], "uci_author": meta["white_uci_author"]})
            identities["B"].update({"uci_name": meta["black_uci_name"], "uci_author": meta["black_uci_author"]})
        else:
            identities["A"].update({"uci_name": meta["black_uci_name"], "uci_author": meta["black_uci_author"]})
            identities["B"].update({"uci_name": meta["white_uci_name"], "uci_author": meta["white_uci_author"]})
        details.append({
            "game": i + 1,
            "uci_meta": meta,
            "a_white": a_is_white,
            "white_engine": "A" if a_is_white else "B",
            "result": res,
            "moves": moves,
            "plies": len(moves),
        })
        print(f"[{i + 1}/{args.games}] {res}  (A: {w}W {d}D {l}L)", flush=True)

    summary = summarize(w, d, l)
    data = {
        "tool_version": "1.0.2",
        "engine_a": os.path.abspath(args.engine_a),
        "engine_b": os.path.abspath(args.engine_b),
        "mode_a": args.mode_a,
        "mode_b": args.mode_b,
        "options_a": {k: v for k, v in options_a},
        "options_b": {k: v for k, v in options_b},
        "depth_a": depth_a,
        "depth_b": depth_b,
        "movetime_a": mt_a,
        "movetime_b": mt_b,
        "max_plies": args.max_plies,
        "engine_a_identity": identities["A"],
        "engine_b_identity": identities["B"],
        "summary": summary,
        "games_detail": details,
    }
    if args.rating_b is not None:
        data["reference_rating_b"] = args.rating_b
        data["engine_a_rating_estimate"] = args.rating_b + summary["elo_delta"]
        data["engine_a_rating_ci95"] = [
            args.rating_b + summary["elo_delta_ci95"][0],
            args.rating_b + summary["elo_delta_ci95"][1],
        ]

    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(data, indent=2), encoding="utf-8")

    if args.pgn:
        pgn = Path(args.pgn)
        pgn.parent.mkdir(parents=True, exist_ok=True)
        blocks = []
        for item in details:
            tags = [
                ("Event", "ChessZero benchmark"),
                ("Site", "local"),
                ("Round", str(item["game"])),
                ("White", identities["A"]["uci_name"] if item["white_engine"] == "A" else identities["B"]["uci_name"]),
                ("Black", identities["B"]["uci_name"] if item["white_engine"] == "A" else identities["A"]["uci_name"]),
                ("Result", item["result"]),
            ]
            body_parts = []
            for idx, mv in enumerate(item["moves"]):
                if idx % 2 == 0:
                    body_parts.append(f"{idx // 2 + 1}. {mv}")
                else:
                    body_parts[-1] += f" {mv}"
            body = " ".join(body_parts) + f" {item['result']}"
            blocks.append("".join(f"[{k} \"{v}\"]\n" for k,v in tags) + "\n" + body + "\n")
        pgn.write_text("\n".join(blocks), encoding="utf-8")

    if args.jsonl:
        jsonl = Path(args.jsonl)
        jsonl.parent.mkdir(parents=True, exist_ok=True)
        with jsonl.open("w", encoding="utf-8") as fh:
            for item in details:
                fh.write(json.dumps(item, separators=(",", ":")) + "\n")

    print(json.dumps(data, indent=2))


if __name__ == "__main__":
    try:
        main()
    except (UCIError, ValueError) as exc:
        print(f"match error: {exc}", file=sys.stderr)
        raise SystemExit(2)
