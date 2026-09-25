#!/usr/bin/env python3
import math

def score(wins, draws, losses):
    n=wins+draws+losses
    return (wins+0.5*draws)/n if n else 0.5

def elo_delta(s):
    s=min(max(s,1e-9),1-1e-9)
    return 400.0*math.log10(s/(1.0-s))

def wilson_score_interval(wins, draws, losses, z=1.959963984540054):
    n=wins+draws+losses
    if not n: return (0.5,0.5)
    # Treat each game score as 0, .5, or 1; conservative variance for the mean score.
    valsq=(wins+0.25*draws)/n
    s=score(wins,draws,losses)
    var=max(0.0, valsq-s*s)
    se=math.sqrt(var/n)
    lo=max(1e-6,s-z*se); hi=min(1-1e-6,s+z*se)
    return lo,hi

def summarize(w,d,l):
    s=score(w,d,l); lo,hi=wilson_score_interval(w,d,l)
    return {"games":w+d+l,"wins":w,"draws":d,"losses":l,"score":s,"elo_delta":elo_delta(s),"elo_delta_ci95":[elo_delta(lo),elo_delta(hi)]}
