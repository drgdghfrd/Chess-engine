#include "../src/engine/Search.h"
#include "../src/engine/Evaluation.h"
#include "../src/chess/Position.h"
#include <cassert>
#include <iostream>

using namespace uc;

int main() {
    assert(network().load("nets/ChessZero-v0.75-halfkp.nnue"));
    Position p;
    p.start();

    Search s(32);
    assert(s.configuredHashMB() == 32);
    assert(s.threads() == 1);
    assert(s.totalParallelHashMB() == 32);

    // The configured Hash value is a total budget, not Hash × Threads.
    s.setThreads(7);
    s.setHashMB(256);
    assert(s.threads() == 7);
    assert(s.parallelWorkerCount() == 6);
    assert(s.totalParallelHashMB() <= 256);
    assert(s.totalParallelHashMB() == 256);
    assert(s.allocatedHashMB() == 64); // coordinator gets first balanced upgrade

    // Changing the thread count reconfigures the worker pool while retaining
    // the same total Hash budget.
    s.setThreads(5);
    assert(s.threads() == 5);
    assert(s.parallelWorkerCount() == 4);
    assert(s.totalParallelHashMB() == 256);
    assert(s.parallelWorkerCount() == 4);

    // Changing Hash redistributes the same number of workers without multiplying
    // the requested memory.
    s.setHashMB(128);
    assert(s.configuredHashMB() == 128);
    assert(s.totalParallelHashMB() == 128);
    assert(s.allocatedHashMB() == 32); // balanced layout: 32+32+32+16+16

    // Returning to one thread restores the whole Hash budget to the coordinator.
    s.setThreads(1);
    assert(s.parallelWorkerCount() == 0);
    assert(s.totalParallelHashMB() == 128);

    // Very small Hash values do not force hidden per-thread allocations.
    s.setThreads(7);
    s.setHashMB(1);
    assert(s.threads() == 7);
    assert(s.parallelWorkerCount() == 6);
    assert(s.totalParallelHashMB() == 1);

    // The engine remains functional even when some low-budget workers have no TT.
    s.setHashMB(32);
    Move m = s.go(p, 4);
    assert(m.from < 64 && m.to < 64);
    assert(s.completedDepth() >= 1);

    std::cout << "v1.1.0 runtime resource tests: PASS\n"
              << "threads=" << s.threads()
              << " workers=" << s.parallelWorkerCount()
              << " configured_hash_mb=" << s.configuredHashMB()
              << " allocated_total_mb=" << s.totalParallelHashMB() << "\n";
    return 0;
}
