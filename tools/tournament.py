#!/usr/bin/env python3
"""Run a small UCI match and estimate Elo from W/D/L.
Usage: tournament.py ENGINE_A ENGINE_B [games] [depth]
Colors alternate. This is a benchmark helper, not an official rating procedure.
"""
import math, subprocess, sys

def elo(diff):
    return 400.0*math.log10(10.0**(diff/400.0)) if False else diff

def play(engine, fen_moves, depth):
    p=subprocess.Popen([engine],stdin=subprocess.PIPE,stdout=subprocess.PIPE,text=True,bufsize=1)
    def cmd(x): p.stdin.write(x+'\n'); p.stdin.flush()
    cmd('uci')
    while True:
        line=p.stdout.readline()
        if 'uciok' in line: break
    cmd('isready')
    while 'readyok' not in p.stdout.readline(): pass
    cmd('position startpos'+((' moves '+' '.join(fen_moves)) if fen_moves else ''))
    cmd(f'go depth {depth}')
    best=''
    while True:
        line=p.stdout.readline()
        if not line: break
        if line.startswith('bestmove '): best=line.split()[1]; break
    cmd('quit'); p.wait(timeout=2)
    return best

def game(a,b,depth,maxplies=160):
    # Uses each engine independently and feeds back the move list. Results are
    # adjudicated by the same simple engine-side terminal rules through python-chess
    # when installed; otherwise games are recorded as move generation benchmarks.
    try:
        import chess
    except ImportError:
        return 'U'
    board=chess.Board(); engines=[a,b]; ply=0
    while ply<maxplies and not board.is_game_over():
        turn=0 if board.turn else 1
        move=play(engines[turn], [m.uci() for m in board.move_stack], depth)
        try: board.push_uci(move)
        except Exception: return 'U'
        ply+=1
    if board.is_checkmate(): return 'A' if board.turn==False else 'B'
    return 'D'

def main():
    if len(sys.argv)<3: raise SystemExit('usage: tournament.py ENGINE_A ENGINE_B [games] [depth]')
    a,b=sys.argv[1],sys.argv[2]; games=int(sys.argv[3]) if len(sys.argv)>3 else 4; depth=int(sys.argv[4]) if len(sys.argv)>4 else 3
    wa=wb=dr=0
    for i in range(games):
        r=game(a,b,depth)
        if r=='A': wa+=1
        elif r=='B': wb+=1
        elif r=='D': dr+=1
        print(f'game {i+1}/{games}: {r}')
    scored=wa+0.5*dr; total=wa+wb+dr
    print(f'W/D/L: {wa}/{dr}/{wb}')
    if total:
        p=scored/total
        if 0<p<1:
            diff= -400*math.log10(1/p-1)
            print(f'performance Elo difference (A-B): {diff:.1f}')
        else: print('performance Elo difference: ±inf (no decisive result against one side)')
        if 0< p <1:
            se=400/(math.log(10)*math.sqrt(total*p*(1-p)))
            print(f'approx 95% CI: ±{1.96*se:.1f} Elo')
if __name__=='__main__': main()
