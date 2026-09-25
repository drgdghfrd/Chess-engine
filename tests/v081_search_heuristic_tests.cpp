#include "../src/chess/Position.h"
#include "../src/engine/Search.h"
#include "../src/nnue/NNUE.h"
#include <cassert>
#include <iostream>
using namespace uc;
static bool legalMove(const Position& p,const Move& target){
    if(target.from==target.to && target.flag==Flag::Quiet) return false;
    for(const Move& m:p.legal()) if(m==target) return true;
    return false;
}
int main(){
    assert(network().load("nets/ChessZero-v0.75-halfkp.nnue"));
    Position p; p.start();
    Search s(32);
    s.setHistoryPruning(true); s.setHistoryPruningAggression(100); s.setFutilityAggression(100);
    Move m=s.go(p,4);
    assert(legalMove(p,m));
    assert(s.completedDepth()>=1 && s.nodes()>0);

    assert(s.historyPruningEnabled());
    assert(s.historyPruningAggression()==100);
    assert(s.futilityAggression()==100);
    s.setHistoryPruning(false); assert(!s.historyPruningEnabled());
    s.setHistoryPruningAggression(50); assert(s.historyPruningAggression()==50);
    s.setHistoryPruningAggression(250); assert(s.historyPruningAggression()==200);
    s.setFutilityAggression(50); assert(s.futilityAggression()==50);
    s.setFutilityAggression(250); assert(s.futilityAggression()==200);

    std::cout << "v0.81 Search Heuristic tuning tests: PASS\n";
    std::cout << "history_prunes=" << s.historyPrunes()
              << " futility_prunes=" << s.futilityPrunes() << "\n";
}
