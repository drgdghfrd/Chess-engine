#!/usr/bin/env python3
import json
import os
import stat
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RUNNER = ROOT / "tools" / "match" / "run_match.py"

MOCK = r'''#!/usr/bin/env python3
import sys
ROLE = "__ROLE__"
moves = []
for line in sys.stdin:
    line = line.strip()
    if line == "uci":
        print("id name Mock" + ROLE, flush=True)
        print("uciok", flush=True)
    elif line.startswith("setoption"):
        pass
    elif line == "isready":
        print("readyok", flush=True)
    elif line == "ucinewgame":
        moves = []
    elif line.startswith("position "):
        if " moves " in line:
            moves = line.split(" moves ", 1)[1].split()
        else:
            moves = []
    elif line.startswith("go "):
        print("info depth 1 score cp 0 nodes 1", flush=True)
        ply = len(moves)
        seq = {0: "f2f3", 1: "e7e5", 2: "g2g4", 3: "d8h4"}
        mv = seq.get(ply, "(none)")
        print("bestmove " + mv, flush=True)
    elif line == "quit":
        break
'''


def make_engine(path, role):
    path.write_text(MOCK.replace("__ROLE__", role), encoding="utf-8")
    path.chmod(path.stat().st_mode | stat.S_IEXEC)


def main():
    with tempfile.TemporaryDirectory() as td:
        td = Path(td)
        a = td / "engine_a.py"
        b = td / "engine_b.py"
        make_engine(a, "A")
        make_engine(b, "B")
        out = td / "match.json"
        proc = subprocess.run(
            [
                sys.executable, str(RUNNER),
                "--engine-a", str(a),
                "--engine-b", str(b),
                "--depth", "1",
                "--games", "2",
                "--max-plies", "20",
                "--option-a", "Mock=1",
                "--option-b", "Mock=2",
                "--rating-b", "3000",
                "--out", str(out),
            ],
            cwd=str(ROOT),
            text=True,
            capture_output=True,
        )
        if proc.returncode != 0:
            print(proc.stdout)
            print(proc.stderr, file=sys.stderr)
            raise SystemExit(proc.returncode)
        data = json.loads(out.read_text(encoding="utf-8"))
        assert data["tool_version"] == "1.0.2"
        assert data["engine_a"] == str(a.resolve())
        assert data["engine_b"] == str(b.resolve())
        assert data["options_a"]["Mock"] == "1"
        assert data["options_b"]["Mock"] == "2"
        assert data["summary"]["games"] == 2
        assert len(data["games_detail"]) == 2
        assert data["summary"]["wins"] == 1
        assert data["summary"]["losses"] == 1
        assert data["summary"]["draws"] == 0
        assert all(g["result"] == "0-1" for g in data["games_detail"])
        assert all(g["plies"] == 4 for g in data["games_detail"])
        assert "engine_a_rating_estimate" in data
        print("v1.0.2 cross-engine match runner: PASS")
        print("distinct_executables=true local_adjudication=true rating_offset=true")


if __name__ == "__main__":
    main()
