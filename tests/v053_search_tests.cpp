#include "../src/chess/Position.h"
#include "../src/engine/Search.h"
#include "../src/nnue/NNUE.h"
#include <cassert>
#include <iostream>

using namespace uc;

static bool legalMove(const Position& p, const Move& target) {
    if (target.from == target.to && target.flag == Flag::Quiet) return false;
    for (const Move& m : p.legal()) if (m == target) return true;
    return false;
}

int main() {
    network().load("../nets/ChessZero-v0.32-demo.nnue");
    Search search(32);

    Position start;
    start.start();
    Move a = search.go(start, 7);
    assert(legalMove(start, a));
    assert(search.completedDepth() >= 1);
    assert(search.nodes() > 0);

    // Warm TT/PV/history and force another PVS search from the same root.
    Move b = search.go(start, 7);
    assert(legalMove(start, b));
    assert(search.completedDepth() >= 1);
    assert(search.nodes() > 0);

    // Tactical position: aspiration windows must recover from fail-high/low.
    Position tactical;
    assert(tactical.setFEN("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N1BQ1p/PPPBBPPP/R3K2R w KQkq - 0 1"));
    Move c = search.go(tactical, 7);
    assert(legalMove(tactical, c));
    assert(search.completedDepth() >= 1);

    // Hard-clock path must still return a legal move while PVS is active.
    Position blitz;
    blitz.start();
    Move d = search.goTimed(blitz, 14, 120, 120, 10, 10, 20);
    assert(legalMove(blitz, d));

    // v0.54: repeated short-clock iterative deepening must keep a legal move
    // and a completed depth instead of exposing a partially searched depth.
    Position timedStable;
    timedStable.start();
    Move e = search.goTimed(timedStable, 18, 300, 300, 20, 20, 25);
    assert(legalMove(timedStable, e));
    assert(search.completedDepth() >= 1);

    std::cout << "v0.54 iterative-deepening/time-budget tests: PASS\n";
    std::cout << "moves=" << toUci(a) << ',' << toUci(b) << ',' << toUci(c) << ',' << toUci(d) << ',' << toUci(e) << "\n";
    return 0;
}
