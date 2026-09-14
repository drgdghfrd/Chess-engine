#!/usr/bin/env python3
"""UltraChess Arena: dependency-free UCI engine-vs-engine runner.

CLI:
  python3 tools/arena.py --engine-a ./ultrachess --engine-b ./stockfish --games 4 --depth 5
GUI:
  python3 tools/arena.py --gui

The GUI uses the same controller and displays the board, moves, result and log.
Game adjudication uses an UltraChess validator (perft/status via UCI), plus
threefold repetition, 50-move rule, insufficient material, and a max-ply cap.
"""
from __future__ import annotations
import argparse, math, os, queue, subprocess, sys, threading, time
from dataclasses import dataclass
from pathlib import Path

STARTPOS = "startpos"
UNICODE = {
    "P":"♙","N":"♘","B":"♗","R":"♖","Q":"♕","K":"♔",
    "p":"♟","n":"♞","b":"♝","r":"♜","q":"♛","k":"♚",
}

class UCIError(RuntimeError): pass

class UCIEngine:
    def __init__(self, path, label=None):
        self.path=str(path); self.label=label or Path(self.path).name
        self.p=None; self.lock=threading.Lock(); self.alive=False
    def start(self):
        self.p=subprocess.Popen([self.path], stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                                stderr=subprocess.STDOUT, text=True, bufsize=1)
        self.alive=True
        self._cmd("uci")
        self._wait_for("uciok", 10)
        self._cmd("isready")
        self._wait_for("readyok", 10)
    def _cmd(self, s):
        if not self.p or self.p.poll() is not None: raise UCIError(f"{self.label} not running")
        self.p.stdin.write(s+"\n"); self.p.stdin.flush()
    def _read_until(self, predicate, timeout):
        end=time.time()+timeout
        lines=[]
        while time.time()<end:
            line=self.p.stdout.readline()
            if not line: break
            line=line.rstrip('\n'); lines.append(line)
            if predicate(line): return lines
        raise UCIError(f"timeout waiting for {self.label}: {lines[-5:]}")
    def _wait_for(self, token, timeout): return self._read_until(lambda l: token in l, timeout)
    def option(self, name, value):
        self._cmd(f"setoption name {name} value {value}")
        self._cmd("isready"); self._wait_for("readyok", 10)
    def bestmove(self, moves, depth=None, movetime=None, threads=None):
        if threads is not None:
            try: self.option("Threads", threads)
            except UCIError: pass
        self._cmd("position startpos" + ((" moves " + " ".join(moves)) if moves else ""))
        if movetime is not None: self._cmd(f"go movetime {max(1,int(movetime))}")
        else: self._cmd(f"go depth {max(1,int(depth or 4))}")
        for line in self._read_until(lambda l:l.startswith("bestmove "), 120):
            if line.startswith("bestmove "):
                return line.split()[1]
        raise UCIError("bestmove missing")
    def close(self):
        if not self.p: return
        try:
            self._cmd("quit"); self.p.wait(timeout=2)
        except Exception:
            try: self.p.kill()
            except Exception: pass
        self.alive=False

@dataclass
class GameResult:
    result:str
    plies:int
    moves:list[str]
    reason:str

class Validator:
    def __init__(self, path): self.eng=UCIEngine(path, "validator")
    def start(self): self.eng.start()
    def status(self, moves):
        self.eng._cmd("position startpos" + ((" moves "+" ".join(moves)) if moves else ""))
        self.eng._cmd("d")
        lines=self.eng._read_until(lambda l:l.startswith("info string status="), 10)
        status_line=next(l for l in lines if l.startswith("info string status="))
        vals={}
        for tok in status_line.split()[2:]:
            if '=' in tok:
                k,v=tok.split('=',1); vals[k]=v
        fen=next((l for l in lines if l and l.split()[0].count('/')==7), None)
        return vals.get('status','play'), int(vals.get('legal','0')), fen or ""
    def close(self): self.eng.close()

def fen_key(fen):
    return " ".join(fen.split()[:4])

def board_from_fen(fen):
    rows=fen.split()[0].split('/'); b={}
    for r,row in enumerate(rows):
        file=0
        for c in row:
            if c.isdigit(): file += int(c)
            else: b[(7-r,file)] = c; file += 1
    return b

def insufficient(fen):
    b=board_from_fen(fen); pieces=[p for p in b.values() if p.lower()!='k']
    if not pieces: return True
    if any(p.lower() in ('p','q','r') for p in pieces): return False
    if len(pieces)==1 and pieces[0].lower() in ('b','n'): return True
    # King+bishop vs king+bishop with bishops on same color.
    if len(pieces)==2 and all(p.lower()=='b' for p in pieces):
        bishops=[]
        for sq,p in b.items():
            if p.lower()=='b': bishops.append((sq[0]+sq[1])%2)
        return bishops[0]==bishops[1]
    return False

def play_game(a,b,validator,depth=4,movetime=None,maxplies=300,book_a=False,book_b=False,threads=1,stop_event=None, on_ply=None):
    engines=[a,b]; books=[book_a,book_b]; moves=[]; seen={};
    result='1/2-1/2'; reason='max plies'
    status,_,fen=validator.status(moves); seen[fen_key(fen)]=1
    for ply in range(maxplies):
        if stop_event and stop_event.is_set(): return GameResult('abort',ply,moves,'stopped')
        idx=ply%2; e=engines[idx]
        try:
            if books[idx]:
                # Engines that support UseBook get the setting; unsupported engines just search.
                try: e.option('UseBook','true')
                except UCIError: pass
            else:
                try: e.option('UseBook','false')
                except UCIError: pass
            mv=e.bestmove(moves,depth=depth,movetime=movetime,threads=threads)
        except Exception as ex:
            return GameResult('abort',ply,moves,f'{e.label}: {ex}')
        if not mv or mv in ('0000','(none)'):
            result='0-1' if idx==0 else '1-0'; reason='no legal move'; return GameResult(result,ply,moves,reason)
        moves.append(mv)
        if on_ply: on_ply(ply+1, idx, mv, moves[:])
        status,legal,fen=validator.status(moves)
        k=fen_key(fen); seen[k]=seen.get(k,0)+1
        if status=='checkmate':
            result='0-1' if idx==0 else '1-0'; reason='checkmate'; break
        if status=='stalemate': result='1/2-1/2'; reason='stalemate'; break
        if fen:
            half=int(fen.split()[4]);
            if half>=100: result='1/2-1/2'; reason='50-move rule'; break
            if seen[k]>=3: result='1/2-1/2'; reason='threefold repetition'; break
            if insufficient(fen): result='1/2-1/2'; reason='insufficient material'; break
    return GameResult(result,len(moves),moves,reason)

def elo_diff(score, games):
    if games<=0 or score<=0 or score>=games: return None
    return -400*math.log10(1/score-1/games)

def run_match(args, progress=print):
    a=UCIEngine(args.engine_a,'A:'+Path(args.engine_a).name); b=UCIEngine(args.engine_b,'B:'+Path(args.engine_b).name)
    v=Validator(args.validator or args.engine_a)
    for x in (a,b,v): x.start()
    wa=wb=dr=0; allres=[]
    try:
        for i in range(args.games):
            if args.alternate and i%2:
                ea, eb = b, a
                eba, ebb = args.book_b, args.book_a
            else:
                ea, eb = a, b
                eba, ebb = args.book_a, args.book_b
            r=play_game(ea,eb,v,args.depth,args.movetime,args.maxplies,eba,ebb,args.threads)
            allres.append(r); 
            if r.result=='1-0': wa+=1
            elif r.result=='0-1': wb+=1
            elif r.result=='1/2-1/2': dr+=1
            progress(f"Game {i+1}/{args.games}: {r.result} ({r.reason}, {r.plies} plies)")
            progress("  "+" ".join(r.moves))
    finally:
        for x in (a,b,v): x.close()
    games=wa+wb+dr; score=wa+0.5*dr; d=elo_diff(score,games)
    progress(f"W/D/L: {wa}/{dr}/{wb}")
    progress("Performance Elo A-B: " + (f"{d:.1f}" if d is not None else "undefined (extreme score)"))
    return allres

def gui(args):
    import tkinter as tk
    from tkinter import ttk, filedialog, messagebox
    root=tk.Tk(); root.title('UltraChess Arena v0.11'); root.geometry('1050x720')
    board=tk.Canvas(root,width=520,height=520,highlightthickness=0); board.grid(row=0,column=0,rowspan=8,padx=10,pady=10)
    right=ttk.Frame(root); right.grid(row=0,column=1,sticky='nsew',padx=10,pady=10); root.columnconfigure(1,weight=1)
    def ent(label, val, row):
        ttk.Label(right,text=label).grid(row=row,column=0,sticky='w'); e=ttk.Entry(right,width=42); e.insert(0,val); e.grid(row=row,column=1,sticky='ew'); return e
    ea=ent('Engine A',args.engine_a or 'ultrachess',0); eb=ent('Engine B',args.engine_b or 'ultrachess',1); ev=ent('Validator',args.validator or '',2)
    gd=ent('Depth',str(args.depth),3); gm=ent('Move time ms',str(args.movetime or 0),4); gp=ent('Max plies',str(args.maxplies),5); gt=ent('Threads',str(args.threads),6)
    ba=tk.BooleanVar(value=args.book_a); bb=tk.BooleanVar(value=args.book_b)
    ttk.Checkbutton(right,text='Book A',variable=ba).grid(row=7,column=0,sticky='w'); ttk.Checkbutton(right,text='Book B',variable=bb).grid(row=7,column=1,sticky='w')
    log=tk.Text(right,width=58,height=25); log.grid(row=8,column=0,columnspan=2,pady=8,sticky='nsew'); right.rowconfigure(8,weight=1)
    moves_var=tk.StringVar(value=''); ttk.Label(right,textvariable=moves_var,wraplength=530).grid(row=9,column=0,columnspan=2,sticky='w')
    buttons=ttk.Frame(right); buttons.grid(row=10,column=0,columnspan=2,sticky='w')
    stop=threading.Event()
    def draw(fen):
        board.delete('all'); sq=65
        for rank in range(8):
            for file in range(8):
                x=file*sq;y=(7-rank)*sq; board.create_rectangle(x,y,x+sq,y+sq)
        for (rank,file),pc in board_from_fen(fen).items():
            board.create_text(file*sq+sq/2,(7-rank)*sq+sq/2,text=UNICODE.get(pc,pc),font=('Arial',38))
    def start_match():
        stop.clear(); log.delete('1.0','end');
        try: depth=int(gd.get()); mt=int(gm.get()) or None; maxp=int(gp.get()); th=int(gt.get())
        except ValueError: messagebox.showerror('Input','Depth/movetime/max plies/threads must be numbers'); return
        def worker():
            A=UCIEngine(ea.get(),'A'); B=UCIEngine(eb.get(),'B'); V=Validator(ev.get() or ea.get())
            try:
                for x in (A,B,V): x.start()
                def ply_cb(n,idx,mv,ms):
                    def update():
                        log.insert('end',f'{n}. {"White" if idx==0 else "Black"}: {mv}\n'); log.see('end'); moves_var.set(' '.join(ms))
                        try:
                            st,_,fen=V.status(ms); draw(fen)
                        except Exception: pass
                    root.after(0,update)
                r=play_game(A,B,V,depth,mt,maxp,ba.get(),bb.get(),th,stop,ply_cb)
                root.after(0,lambda: log.insert('end',f'\nRESULT {r.result} — {r.reason}\n'))
            except Exception as ex:
                root.after(0,lambda: log.insert('end',f'ERROR: {ex}\n'))
            finally:
                for x in (A,B,V): x.close()
        threading.Thread(target=worker,daemon=True).start()
    ttk.Button(buttons,text='Start Match',command=start_match).pack(side='left',padx=3)
    ttk.Button(buttons,text='Stop',command=stop.set).pack(side='left',padx=3)
    draw('8/8/8/8/8/8/8/8 w - - 0 1')
    root.mainloop()

def main():
    ap=argparse.ArgumentParser(); ap.add_argument('--gui',action='store_true'); ap.add_argument('--engine-a'); ap.add_argument('--engine-b'); ap.add_argument('--validator'); ap.add_argument('--games',type=int,default=2); ap.add_argument('--depth',type=int,default=4); ap.add_argument('--movetime',type=int,default=0); ap.add_argument('--maxplies',type=int,default=300); ap.add_argument('--threads',type=int,default=1); ap.add_argument('--book-a',action='store_true'); ap.add_argument('--book-b',action='store_true'); ap.add_argument('--alternate',action='store_true')
    a=ap.parse_args()
    if a.gui: return gui(a)
    if not a.engine_a or not a.engine_b: ap.error('--engine-a and --engine-b are required unless --gui')
    if not a.validator: a.validator=a.engine_a
    run_match(a)
if __name__=='__main__': main()
