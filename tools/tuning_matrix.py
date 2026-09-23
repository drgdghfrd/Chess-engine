#!/usr/bin/env python3
"""ChessZero v0.83/v0.84 heuristic tuning matrix.
Runs one fresh engine process per configuration and records observations.
This tool deliberately does not rank configurations or estimate Elo.
"""
from __future__ import annotations
import argparse, csv, re, subprocess, time

FENS = [
    "r1bq1rk1/ppp2ppp/2n1pn2/8/2BPP3/2N2N2/PPP2PPP/R1BQ1RK1 b - - 4 8",
    "r2q1rk1/pp1nbppp/2p1pn2/8/2B1P3/2N1BN2/PPP2PPP/R2Q1RK1 w - - 0 10",
    "8/5pk1/2p2q2/1p1p4/3P4/2P2Q2/5PK1/8 w - - 0 1",
]
HEURISTICS = {
    "LMR": "LMRAggression",
    "NullMove": "NullMoveAggression",
    "Razoring": "RazoringAggression",
    "History": "HistoryAggression",
    "Countermove": "CountermoveAggression",
    "HistoryPruning": "HistoryPruningAggression",
    "Futility": "FutilityAggression",
}
VALUES = [50, 75, 100, 125, 150, 200]

def measure(engine, network, depth, options):
    cmds=["uci", "setoption name UseBook value false", f"setoption name EvalFile value {network}"]
    cmds += options + ["isready"]
    for fen in FENS: cmds += ["position fen "+fen, f"go depth {depth}"]
    cmds += ["quit"]
    t0=time.perf_counter()
    cp=subprocess.run([engine], input="\n".join(cmds)+"\n", text=True, stdout=subprocess.PIPE,
                      stderr=subprocess.DEVNULL, timeout=180)
    elapsed=(time.perf_counter()-t0)*1000
    pending=None; rows=[]
    for line in cp.stdout.splitlines():
        m=re.search(r"info depth (\d+) score cp (-?\d+) nodes (\d+)(?: nps (\d+))?", line)
        if m: pending=(int(m.group(1)),int(m.group(2)),int(m.group(3)),int(m.group(4) or 0))
        if line.startswith("bestmove "):
            if pending is None: raise RuntimeError("missing info before bestmove")
            rows.append(pending); pending=None
    if len(rows)!=len(FENS): raise RuntimeError(f"expected {len(FENS)} searches, got {len(rows)}")
    return rows, elapsed

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("engine"); ap.add_argument("network")
    ap.add_argument("--depth",type=int,default=6)
    ap.add_argument("--heuristic",choices=["all"]+list(HEURISTICS),default="all")
    ap.add_argument("--values",default="50,75,100,125,150,200")
    ap.add_argument("--csv",default="")
    args=ap.parse_args()
    vals=[int(x) for x in args.values.split(",")]
    names=list(HEURISTICS) if args.heuristic=="all" else [args.heuristic]
    configs=[("baseline",[])]
    for h in names:
        opt=HEURISTICS[h]
        for v in vals: configs.append((f"{h}_{v}",[f"setoption name {opt} value {v}"]))
    out=[]
    for name,opts in configs:
        rows,wall=measure(args.engine,args.network,args.depth,opts)
        for i,(d,s,n,nps) in enumerate(rows,1):
            out.append(dict(config=name,heuristic=name.rsplit("_",1)[0] if "_" in name else "baseline",
                             position=i,depth=d,score_cp=s,nodes=n,nps=nps,wall_ms=round(wall,2)))
    fields=["config","heuristic","position","depth","score_cp","nodes","nps","wall_ms"]
    if args.csv:
        with open(args.csv,"w",newline="") as f:
            w=csv.DictWriter(f,fieldnames=fields); w.writeheader(); w.writerows(out)
    print(",".join(fields))
    for r in out: print(",".join(str(r[k]) for k in fields))

if __name__=="__main__": main()
