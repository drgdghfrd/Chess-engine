#include "../src/chess/Position.h"
#include "../src/engine/Search.h"
#include "../src/nnue/NNUE.h"
#include <cassert>
#include <iostream>

using namespace uc;

static bool legalMove(const Position& p, const Move& target) {
    if (target.from == target.to && target.flag == Flag::Quiet) return false;
    for (const Move& m : p.legal())
        if (m == target) return true;
    return false;
}

int main() {
    network().load("../nets/ChessZero-v0.32-demo.nnue");
    Search search(32);

    Position start;
    start.start();
    Move a = search.go(start, 6);
    assert(legalMove(start, a));
    assert(search.completedDepth() >= 1);
    assert(search.nodes() > 0);
    const uint64_t firstNodes = search.nodes();

    // Warm heuristic tables with a second search from the same root.
    Move b = search.go(start, 6);
    assert(legalMove(start, b));
    assert(search.completedDepth() >= 1);
    assert(search.nodes() > 0);

    // Tactical position exercises PVS/LMR, SEE pruning and qsearch checks.
    Position tactical;
    assert(tactical.setFEN("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1"));
    Move c = search.go(tactical, 7);
    assert(legalMove(tactical, c));
    assert(search.completedDepth() >= 1);

    Position checkLine;
    assert(checkLine.setFEN("r3k2r/ppp2ppp/2n1bn2/8/8/2N1BN2/PPP2PPP/R3K2R w KQkq - 0 1"));
    Move d = search.go(checkLine, 6);
    assert(legalMove(checkLine, d));

    // Short-clock safety must survive the new pruning layer.
    Position blitz;
    blitz.start();
    Move e = search.goTimed(blitz, 14, 100, 100, 10, 10, 20);
    assert(legalMove(blitz, e));
    assert(search.nodes() > 0);

    std::cout << "v0.52 heuristic search tests: PASS\n";
    std::cout << "moves=" << toUci(a) << ',' << toUci(b) << ','
              << toUci(c) << ',' << toUci(d) << ',' << toUci(e) << "\n";
    std::cout << "first_nodes=" << firstNodes << " timed_nodes=" << search.nodes() << "\n";
    return 0;
}
