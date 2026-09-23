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

    // Baseline: both pruning families enabled.
    Position p; p.start();
    Search s(32);
    s.setNullMove(true); s.setNullMoveAggression(100); s.setNullMoveVerification(true);
    s.setRazoring(true); s.setRazoringAggression(100);
    Move m=s.go(p,8);
    assert(legalMove(p,m));
    assert(s.completedDepth()>=1 && s.nodes()>0);
    assert(s.nullMoveSearches()>0);

    // Null move can be disabled independently without affecting legality.
    Position p2; p2.start();
    Search noNull(32); noNull.setNullMove(false);
    Move m2=noNull.go(p2,7);
    assert(legalMove(p2,m2));
    assert(noNull.nullMoveSearches()==0);
    assert(noNull.nullMoveCutoffs()==0);

    // Verification is independently switchable.
    Position p3; p3.start();
    Search noVerify(32); noVerify.setNullMove(true); noVerify.setNullMoveVerification(false);
    Move m3=noVerify.go(p3,8);
    assert(legalMove(p3,m3));
    assert(noVerify.nullMoveVerifications()==0);

    // Razoring is independently switchable and must remain a safe qsearch gate.
    Position p4; p4.setFEN("8/8/8/8/8/8/1k6/7K w - - 0 1");
    Search razorOff(32); razorOff.setRazoring(false);
    Move m4=razorOff.go(p4,4);
    assert(legalMove(p4,m4));
    assert(razorOff.razorSearches()==0);

    Position p5; p5.start();
    Search aggressive(32); aggressive.setNullMoveAggression(150); aggressive.setRazoringAggression(150);
    Move m5=aggressive.go(p5,7);
    assert(legalMove(p5,m5));
    assert(aggressive.nullMoveAggression()==150);
    assert(aggressive.razoringAggression()==150);

    std::cout << "v0.79 Null Move + Razoring tuning tests: PASS\n";
    std::cout << "null_searches=" << s.nullMoveSearches()
              << " null_cutoffs=" << s.nullMoveCutoffs()
              << " null_verifications=" << s.nullMoveVerifications()
              << " null_verify_failures=" << s.nullMoveVerificationFailures() << "\n";
    std::cout << "razor_searches=" << s.razorSearches()
              << " razor_fail_lows=" << s.razorFailLows() << "\n";
}
