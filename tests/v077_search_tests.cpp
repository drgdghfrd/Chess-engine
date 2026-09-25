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

    Search aspirationSearch(32);
    aspirationSearch.setAspiration(true);
    aspirationSearch.setAspirationWindow(20);
    Position start; start.start();
    Move a=aspirationSearch.go(start,7);
    assert(legalMove(start,a));
    assert(aspirationSearch.completedDepth()>=1);
    assert(aspirationSearch.nodes()>0);
    assert(aspirationSearch.aspirationSearches()>=1);

    Search fullWindowSearch(32);
    fullWindowSearch.setAspiration(false);
    Position start2; start2.start();
    Move b=fullWindowSearch.go(start2,6);
    assert(legalMove(start2,b));
    assert(fullWindowSearch.completedDepth()>=1);
    assert(fullWindowSearch.aspirationSearches()==0);

    std::cout<<"v0.77 aspiration/PVS refinement tests: PASS\n";
    std::cout<<"aspiration_searches="<<aspirationSearch.aspirationSearches()
             <<" fail_low="<<aspirationSearch.aspirationFailLows()
             <<" fail_high="<<aspirationSearch.aspirationFailHighs()
             <<" pvs_research="<<aspirationSearch.pvsResearches()<<"\n";
    return 0;
}
