#include "engine/Search.h"
#include "chess/Position.h"
#include "nnue/NNUE.h"
#include <cassert>
#include <iostream>

using namespace uc;

int main(){
    assert(network().load("nets/ChessZero-v0.75-halfkp.nnue"));
    Position p;
    p.start();

    Search single(32);
    single.setThreads(1);
    Move a=single.go(p,5);
    assert(a.from<64 && a.to<64);
    assert(single.completedDepth()>=1);
    assert(single.lazySmpLaunches()==0);

    Search parallel(32);
    parallel.setThreads(4);
    assert(parallel.threads()==4);
    Move b=parallel.go(p,5);
    assert(b.from<64 && b.to<64);
    assert(parallel.completedDepth()>=1);
    assert(parallel.lazySmpLaunches()>0);
    assert(parallel.lazySmpCompleted()>0);

    std::cout << "v0.86 Lazy SMP tests: PASS\n"
              << "threads=" << parallel.threads()
              << " launches=" << parallel.lazySmpLaunches()
              << " completed=" << parallel.lazySmpCompleted()
              << " nodes=" << parallel.nodes() << "\n";
    return 0;
}
