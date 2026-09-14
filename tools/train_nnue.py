#!/usr/bin/env python3
import csv, math, struct, sys, random
INPUTS,HIDDEN=768,128
MAGIC,VERSION=0x3445554E,3
VALUES=[100,320,330,500,900,0]

def features(fen):
    board=fen.split()[0]; out=[0]*INPUTS; sq=56
    types={'P':0,'N':1,'B':2,'R':3,'Q':4,'K':5,'p':6,'n':7,'b':8,'r':9,'q':10,'k':11}
    for c in board:
        if c=='/': sq-=16
        elif c.isdigit(): sq+=int(c)
        else: out[types[c]*64+sq]=1; sq+=1
    return out

def main(inp,out,epochs):
    rows=[]
    with open(inp,newline='',encoding='utf-8') as f:
        for row in csv.DictReader(f): rows.append((features(row['fen']),float(row['score'])))
    if not rows: raise SystemExit('empty dataset')
    rng=random.Random(7)
    w1=[[rng.uniform(-0.02,0.02) for _ in range(INPUTS)] for _ in range(HIDDEN)]
    b1=[0.0]*HIDDEN; w2=[rng.uniform(-0.02,0.02) for _ in range(HIDDEN)]; b2=0.0
    lr=0.00001
    for ep in range(epochs):
        rng.shuffle(rows); loss=0.0
        for x,y in rows:
            z=[b1[h]+sum(w*xv for w,xv in zip(w1[h],x)) for h in range(HIDDEN)]
            a=[max(0.0,v) for v in z]
            pred=b2+sum(u*v for u,v in zip(w2,a)); err=max(-10000,min(10000,pred-y)); loss+=err*err
            go=2*err
            for h in range(HIDDEN):
                gh=go*w2[h]*(1.0 if z[h]>0 else 0.0)
                w2[h]-=lr*go*a[h]; b1[h]-=lr*gh
                if gh:
                    for i,xi in enumerate(x):
                        if xi: w1[h][i]-=lr*gh
            b2-=lr*go
        print(f'epoch {ep+1}/{epochs} mse={loss/len(rows):.2f}')
    with open(out,'wb') as f:
        f.write(struct.pack('<II',MAGIC,VERSION))
        for h in range(HIDDEN):
            for v in w1[h]: f.write(struct.pack('<h',max(-32768,min(32767,round(v*1024)))))
        for v in b1: f.write(struct.pack('<h',max(-32768,min(32767,round(v*1024)))))
        for v in w2: f.write(struct.pack('<h',max(-32768,min(32767,round(v*1024)))))
        f.write(struct.pack('<i',round(b2*16)))
if __name__=='__main__':
    if len(sys.argv)<3: raise SystemExit('usage: train_nnue.py dataset.csv output.nnue [epochs]')
    main(sys.argv[1],sys.argv[2],int(sys.argv[3]) if len(sys.argv)>3 else 5)
