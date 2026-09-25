#include "../src/chess/Position.h"
#include "../src/engine/Search.h"
#include "../src/nnue/NNUE.h"
#include <cassert>
#include <iostream>
#include <string>
using namespace uc;
static bool legalMove(const Position& p,const Move& target){
    for(const Move& m:p.legal()) if(m==target) return true;
    return false;
}
static uint64_t run(Search& s, const std::string& fen, bool lmr, bool nullMove, bool historyPruning, bool razoring){
    Position p; assert(p.setFEN(fen));
    s.clearHash();
    s.setLMR(lmr); s.setNullMove(nullMove); s.setHistoryPruning(historyPruning); s.setRazoring(razoring);
    Move m=s.go(p,2);
    assert(legalMove(p,m));
    assert(s.completedDepth()>=1 && s.nodes()>0);
    return s.nodes();
}
int main(){
    assert(network().load("nets/ChessZero-v0.75-halfkp.nnue"));
    const std::string fen="r1bq1rk1/ppp2ppp/2n1pn2/8/2BPP3/2N2N2/PPP2PPP/R1BQ1RK1 b - - 4 8";
    Search s(32);
    const uint64_t base=run(s,fen,true,true,true,true);
    const uint64_t noLmr=run(s,fen,false,true,true,true);
    const uint64_t noNull=run(s,fen,true,false,true,true);
    const uint64_t noHist=run(s,fen,true,true,false,true);
    const uint64_t noRazor=run(s,fen,true,true,true,false);
    assert(base>0 && noLmr>0 && noNull>0 && noHist>0 && noRazor>0);
    std::cout << "v0.82 measurement harness tests: PASS\n";
    std::cout << "nodes baseline=" << base << " no_lmr=" << noLmr
              << " no_null=" << noNull << " no_history_pruning=" << noHist
              << " no_razoring=" << noRazor << "\n";
    std::cout << "Note: measurements are observations, not rankings.\n";
}
