#!/usr/bin/env python3
"""Repeatable UCI search measurement for ChessZero v0.82.

Runs the same FENs in fresh engine processes so TT state cannot leak between
configurations. This is a measurement harness, not an Elo predictor.
"""
from __future__ import annotations
import argparse, csv, re, subprocess, time

FENS = [
    "r1bq1rk1/ppp2ppp/2n1pn2/8/2BPP3/2N2N2/PPP2PPP/R1BQ1RK1 b - - 4 8",
    "r2q1rk1/pp1nbppp/2p1pn2/8/2B1P3/2N1BN2/PPP2PPP/R2Q1RK1 w - - 0 10",
    "8/5pk1/2p2q2/1p1p4/3P4/2P2Q2/5PK1/8 w - - 0 1",
]

CONFIGS = [
    ("baseline", []),
    ("no_lmr", ["setoption name LMR value false"]),
    ("no_null", ["setoption name NullMove value false"]),
    ("no_history_pruning", ["setoption name HistoryPruning value false"]),
    ("no_razoring", ["setoption name Razoring value false"]),
]

def run(engine: str, network: str, depth: int, name: str, options: list[str]):
    commands = ["uci", "setoption name UseBook value false",
                f"setoption name EvalFile value {network}", *options, "isready"]
    for fen in FENS:
        commands += ["position fen " + fen, f"go depth {depth}"]
    commands += ["quit"]
    t0 = time.perf_counter()
    cp = subprocess.run([engine], input="\n".join(commands)+"\n", text=True,
                        stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, timeout=120)
    elapsed = time.perf_counter()-t0
    rows=[]
    infos=re.findall(r"info depth (\d+) score cp (-?\d+) nodes (\d+)(?: nps (\d+))?", cp.stdout)
    # Keep the last completed info line for each FEN; depths are monotonic per search.
    idx=0; last={}
    for d,score,nodes,nps in infos:
        last[idx]=(int(d),int(score),int(nodes),int(nps or 0))
        # A later info line can belong to the same position; detect by node reset.
        # The harness intentionally only needs one completed line per position.
        if idx < len(FENS)-1 and int(nodes) > 0:
            pass
    # UCI output does not carry a position id, so parse completed bestmove boundaries.
    chunks=cp.stdout.split("bestmove ")[1:]
    if len(chunks)!=len(FENS):
        raise SystemExit(f"{name}: expected {len(FENS)} bestmove lines, got {len(chunks)}")
    for i,ch in enumerate(chunks):
        block=cp.stdout.split("bestmove ", i+1)[i].split("bestmove ")[-1] if False else ""
    # Simpler robust extraction: each bestmove terminates one search; use the final info
    # line immediately before it.
    lines=cp.stdout.splitlines(); pending=None
    completed=[]
    for line in lines:
        m=re.search(r"info depth (\d+) score cp (-?\d+) nodes (\d+)(?: nps (\d+))?", line)
        if m: pending=(int(m.group(1)),int(m.group(2)),int(m.group(3)),int(m.group(4) or 0))
        if line.startswith("bestmove "):
            if pending is None: raise SystemExit(f"{name}: missing info before bestmove")
            completed.append(pending); pending=None
    for i,(d,score,nodes,nps) in enumerate(completed):
        rows.append(dict(config=name,position=i+1,depth=d,score_cp=score,nodes=nodes,nps=nps))
    return rows, elapsed

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("engine"); ap.add_argument("network")
    ap.add_argument("--depth",type=int,default=8)
    ap.add_argument("--csv",default="")
    args=ap.parse_args()
    all_rows=[]
    for name,opts in CONFIGS:
        rows,elapsed=run(args.engine,args.network,args.depth,name,opts)
        for r in rows: r["wall_ms"]=round(elapsed*1000,2)
        all_rows.extend(rows)
    fields=["config","position","depth","score_cp","nodes","nps","wall_ms"]
    if args.csv:
        with open(args.csv,"w",newline="") as f:
            w=csv.DictWriter(f,fieldnames=fields); w.writeheader(); w.writerows(all_rows)
    print("config,position,depth,score_cp,nodes,nps,wall_ms")
    for r in all_rows: print(",".join(str(r[k]) for k in fields))

if __name__=="__main__": main()
