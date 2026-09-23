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
    assert(network().load("../nets/ChessZero-v0.75-halfkp.nnue"));
    Position p; p.start();
    Search s(32);
    s.setLMR(true); s.setLMRAggression(100);
    Move m=s.go(p,8);
    assert(legalMove(p,m));
    assert(s.completedDepth()>=1 && s.nodes()>0);
    assert(s.lmrSearches()>0);

    Search off(32); off.setLMR(false);
    Position p2; p2.start();
    Move m2=off.go(p2,6);
    assert(legalMove(p2,m2));
    assert(off.lmrSearches()==0);

    Search aggressive(32); aggressive.setLMR(true); aggressive.setLMRAggression(150);
    Position p3; p3.start();
    Move m3=aggressive.go(p3,7);
    assert(legalMove(p3,m3));
    assert(aggressive.lmrSearches()>0);

    std::cout << "v0.78 LMR tuning tests: PASS\n";
    std::cout << "lmr_searches=" << s.lmrSearches()
              << " lmr_researches=" << s.lmrResearches() << "\n";
}
