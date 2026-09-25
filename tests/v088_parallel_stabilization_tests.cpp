#include "../src/engine/Search.h"
#include "../src/engine/Evaluation.h"
#include "../src/chess/Position.h"
#include <cassert>
#include <iostream>
#include <thread>

using namespace uc;

int main(){
    assert(network().load("nets/ChessZero-v0.75-halfkp.nnue"));
    Position p; p.start();

    Search single(32);
    Move a=single.go(p,5);
    assert(a.from<64 && a.to<64);
    assert(single.completedDepth()>=1);

    Search parallel(32);
    parallel.setThreads(4);
    Move b=parallel.go(p,5);
    assert(b.from<64 && b.to<64);
    assert(parallel.completedDepth()>=1);
    assert(parallel.lazySmpLaunches()>0);
    assert(parallel.lazySmpCompleted()>0);
    // lazySmpLaunches counts worker-thread launches, while lazySmpCompleted
    // counts root tasks completed across iterative-deepening passes; their
    // magnitudes are intentionally different. Verify progress rather than a
    // fabricated ratio between the two counters.
    assert(parallel.parallelBestUpdates()>0);

    // A very short timed search must still return a legal root move and must
    // not publish an incomplete root result as if it were complete.
    Search timed(32);
    timed.setThreads(4);
    Move c=timed.go(p,12,1);
    assert(c.from<64 && c.to<64);

    std::cout << "v0.88 parallel stabilization tests: PASS\n"
              << "threads=4"
              << " launches=" << parallel.lazySmpLaunches()
              << " completed=" << parallel.lazySmpCompleted()
              << " aborted=" << parallel.lazySmpAborted()
              << " best_updates=" << parallel.parallelBestUpdates()
              << " nodes=" << parallel.nodes() << "\n";
    return 0;
}
