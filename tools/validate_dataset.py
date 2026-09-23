#!/usr/bin/env python3
"""Validate and inspect a ChessZero .czds HalfKP dataset."""
import argparse, struct
MAGIC=b"CZDSV001"

def read(path):
    with open(path,"rb") as f:
        magic, version, max_features, count = struct.unpack("<8sIII", f.read(20))
        if magic != MAGIC or version != 1 or max_features != 60:
            raise ValueError("unsupported dataset header")
        recs=[]
        for i in range(count):
            head=f.read(4)
            if len(head)!=4: raise ValueError(f"truncated record {i}")
            n, stm, target=struct.unpack("<BBh",head)
            if n>60: raise ValueError(f"record {i}: feature count > 60")
            raw=f.read(2*n)
            if len(raw)!=2*n: raise ValueError(f"truncated features at record {i}")
            feats=struct.unpack("<%dH"%n,raw) if n else ()
            if any(x>=40960 for x in feats): raise ValueError(f"record {i}: feature out of range")
            pad=(-((4+2*n)))%4
            if pad: f.read(pad)
            recs.append((n,stm,target,feats))
    return recs

def main():
    ap=argparse.ArgumentParser()
    ap.add_argument("dataset")
    args=ap.parse_args()
    r=read(args.dataset)
    print(f"OK: {len(r)} records")
    print(f"avg features: {sum(x[0] for x in r)/len(r):.2f}")
    print(f"targets: min={min(x[2] for x in r)/32767:.3f} max={max(x[2] for x in r)/32767:.3f}")
    print(f"side-to-move: white={sum(x[1] for x in r)} black={sum(not x[1] for x in r)}")
if __name__=="__main__": main()
