#include "../src/engine/Search.h"
#include "../src/engine/Evaluation.h"
#include "../src/chess/Position.h"
#include <cassert>
#include <chrono>
#include <iostream>
#include <vector>

using namespace uc;

struct Run {
    Move move{};
    int depth = 0;
    uint64_t nodes = 0;
    long long ms = 0;
    uint64_t launches = 0;
    uint64_t completed = 0;
};

static Run run(Position& p, int threads) {
    Search s(32);
    s.setThreads(threads);
    const auto t0 = std::chrono::steady_clock::now();
    Move m = s.go(p, 5);
    const auto t1 = std::chrono::steady_clock::now();
    Run r;
    r.move = m;
    r.depth = s.completedDepth();
    r.nodes = s.nodes();
    r.ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
    r.launches = s.lazySmpLaunches();
    r.completed = s.lazySmpCompleted();
    return r;
}

int main() {
    assert(network().load("nets/ChessZero-v0.75-halfkp.nnue"));
    Position p; p.start();

    // Single-thread is the reference path for the Phase-4 baseline.
    Run r1 = run(p, 1);
    assert(r1.move.from < 64 && r1.move.to < 64);
    assert(r1.depth >= 1);
    assert(r1.nodes > 0);
    assert(r1.launches == 0);

    // Multi-thread paths must complete and return legal root moves.
    Run r2 = run(p, 2);
    Run r4 = run(p, 4);
    assert(r2.move.from < 64 && r2.move.to < 64);
    assert(r4.move.from < 64 && r4.move.to < 64);
    assert(r2.depth >= 1 && r4.depth >= 1);
    assert(r2.nodes > 0 && r4.nodes > 0);
    assert(r2.launches > 0 && r4.launches > 0);
    assert(r2.completed > 0 && r4.completed > 0);

    // Thread-count clamping is part of the UCI contract.
    Search clamp(32);
    clamp.setThreads(0);
    assert(clamp.threads() == 1);
    clamp.setThreads(999);
    assert(clamp.threads() == 64);

    std::cout << "v0.90 multithreading baseline tests: PASS\n"
              << "t1 depth=" << r1.depth << " nodes=" << r1.nodes << " ms=" << r1.ms << "\n"
              << "t2 depth=" << r2.depth << " nodes=" << r2.nodes << " ms=" << r2.ms
              << " launches=" << r2.launches << " completed=" << r2.completed << "\n"
              << "t4 depth=" << r4.depth << " nodes=" << r4.nodes << " ms=" << r4.ms
              << " launches=" << r4.launches << " completed=" << r4.completed << "\n";
    return 0;
}
