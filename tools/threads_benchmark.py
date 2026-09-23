#!/usr/bin/env python3
"""UCI thread-scaling benchmark for ChessZero v1.0.5.

Waits for bestmove before quitting, so asynchronous UCI engines are measured
for actual searches rather than measuring process-startup time.
"""
import argparse
import csv
import subprocess
import statistics
import time


def run(engine, threads, depth, network, repeats):
    rows=[]
    for _ in range(repeats):
        p=subprocess.Popen([engine], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                           stderr=subprocess.PIPE, text=True, bufsize=1)
        def send(cmd):
            p.stdin.write(cmd + "\n"); p.stdin.flush()
        try:
            send("uci")
            send(f"setoption name Threads value {threads}")
            send(f"setoption name EvalFile value {network}")
            send("isready")
            while True:
                line=p.stdout.readline()
                if not line: raise RuntimeError("engine exited before readyok")
                if line.strip()=="readyok": break
            send("position startpos")
            t0=time.perf_counter(); send(f"go depth {depth}")
            nodes=0; depth_done=0; move=""
            while True:
                line=p.stdout.readline()
                if not line: raise RuntimeError("engine exited before bestmove")
                if line.startswith("info "):
                    parts=line.split()
                    for i,x in enumerate(parts[:-1]):
                        if x=="nodes":
                            try: nodes=int(parts[i+1])
                            except ValueError: pass
                        elif x=="depth":
                            try: depth_done=int(parts[i+1])
                            except ValueError: pass
                elif line.startswith("bestmove "):
                    move=line.split()[1]
                    break
            ms=(time.perf_counter()-t0)*1000.0
            send("quit"); p.wait(timeout=5)
            rows.append((depth_done,nodes,ms,move,p.returncode))
        finally:
            if p.poll() is None:
                p.kill(); p.wait()
    return rows


def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("--engine",default="./build/chesszero")
    ap.add_argument("--network",default="nets/ChessZero-v0.75-halfkp.nnue")
    ap.add_argument("--depth",type=int,default=7)
    ap.add_argument("--threads",default="1,2,4,8")
    ap.add_argument("--repeats",type=int,default=3)
    args=ap.parse_args()
    w=csv.writer(__import__("sys").stdout)
    w.writerow(["threads","depth","nodes_median","wall_ms_median","nps_median","bestmove_set"])
    for t in [int(x) for x in args.threads.split(",") if x.strip()]:
        rows=run(args.engine,t,args.depth,args.network,args.repeats)
        med_ms=statistics.median(r[2] for r in rows)
        med_nodes=statistics.median(r[1] for r in rows)
        nps=(med_nodes/(med_ms/1000.0)) if med_ms else 0
        moves=sorted(set(r[3] for r in rows))
        w.writerow([t,rows[-1][0],int(med_nodes),round(med_ms,3),int(nps),";".join(moves)])

if __name__=="__main__":
    main()
