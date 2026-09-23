#!/usr/bin/env python3
"""Analyze ChessZero tuning CSVs without ranking or selecting a winner."""
from __future__ import annotations
import argparse, csv, math, statistics

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("csv")
    ap.add_argument("--report",default="")
    ap.add_argument("--max-score-drift",type=int,default=100)
    args=ap.parse_args()
    with open(args.csv,newline="") as f: rows=list(csv.DictReader(f))
    if not rows: raise SystemExit("CSV is empty")
    groups={}
    for r in rows:
        key=r["config"]; groups.setdefault(key,[]).append(r)
    lines=["ChessZero heuristic measurement report", "", "This report summarizes observations; it does not rank configurations or predict Elo.", ""]
    lines.append("config,positions,median_nodes,mean_nodes,median_nps,mean_score_cp,score_range_cp,stable")
    for name,rs in groups.items():
        nodes=[int(r["nodes"]) for r in rs]; nps=[int(r.get("nps",0)) for r in rs]
        scores=[int(r["score_cp"]) for r in rs]
        drift=max(scores)-min(scores)
        stable=drift <= args.max_score_drift
        lines.append(f"{name},{len(rs)},{statistics.median(nodes):.1f},{statistics.mean(nodes):.1f},{statistics.median(nps):.1f},{statistics.mean(scores):.1f},{drift},{stable}")
    lines += ["", f"stability_threshold_cp={args.max_score_drift}",
              "Interpretation: stability is only a descriptive threshold over score spread in the supplied positions."]
    text="\n".join(lines)+"\n"
    if args.report: open(args.report,"w").write(text)
    print(text,end="")
if __name__=="__main__": main()
