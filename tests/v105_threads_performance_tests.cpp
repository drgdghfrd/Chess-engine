#include "engine/Search.h"
#include "chess/Position.h"
#include "nnue/NNUE.h"
#include <cassert>
#include <iostream>

using namespace uc;

int main(){
    assert(network().load("nets/ChessZero-v0.75-halfkp.nnue"));
    Position p; p.start();

    Search s2(32); s2.setThreads(2);
    Move m2=s2.go(p,6);
    assert(m2.from<64 && m2.to<64);
    assert(s2.completedDepth()>=1);
    assert(s2.lazySmpLaunches()>0);
    assert(s2.lazySmpCompleted()>0);

    Search s4(32); s4.setThreads(4);
    Move m4=s4.go(p,6);
    assert(m4.from<64 && m4.to<64);
    assert(s4.completedDepth()>=1);
    assert(s4.lazySmpLaunches()>0);
    assert(s4.lazySmpCompleted()>0);

    std::cout << "v1.05 thread scaling safety: PASS\n"
              << "t2 launches=" << s2.lazySmpLaunches() << " completed=" << s2.lazySmpCompleted() << " nodes=" << s2.nodes() << "\n"
              << "t4 launches=" << s4.lazySmpLaunches() << " completed=" << s4.lazySmpCompleted() << " nodes=" << s4.nodes() << "\n";
    return 0;
}
