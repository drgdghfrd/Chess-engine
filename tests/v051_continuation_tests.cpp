#include "../src/chess/Position.h"
#include "../src/engine/Search.h"
#include "../src/nnue/NNUE.h"
#include <cassert>
#include <iostream>
#include <string>
#include <vector>

using namespace uc;

static bool legalMove(const Position& p, const Move& target) {
    for (const Move& m : p.legal())
        if (m == target) return true;
    return false;
}

int main() {
    // The packaged v0.32 demo net is optional: search must stay safe with classic fallback.
    network().load("../nets/ChessZero-v0.32-demo.nnue");

    Search search(16);
    Position p;
    p.start();

    // Warm the history tables through several iterative-deepening searches.
    Move first = search.go(p, 5);
    assert(legalMove(p, first));
    assert(search.completedDepth() >= 1);
    assert(search.nodes() > 0);

    const std::string fenAfterFirst = p.fen();
    assert(!fenAfterFirst.empty());

    // Repeat from the same position. This exercises learned continuation/countermove
    // ordering without changing the public Search API.
    Move second = search.go(p, 6);
    assert(legalMove(p, second));
    assert(search.completedDepth() >= 1);
    assert(search.nodes() > 0);

    // Tactical position with several forcing/quiet replies.
    Position t;
    assert(t.setFEN("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1"));
    Move tactical = search.go(t, 6);
    assert(legalMove(t, tactical));
    assert(search.completedDepth() >= 1);

    // Very short timed search must still return a legal move.
    Position blitz;
    blitz.start();
    Move timed = search.goTimed(blitz, 12, 80, 80, 10, 10, 20);
    assert(legalMove(blitz, timed));
    assert(search.nodes() > 0);

    std::cout << "v0.51 continuation history tests: PASS\n";
    std::cout << "start bestmove=" << toUci(first)
              << " repeat=" << toUci(second)
              << " tactical=" << toUci(tactical)
              << " timed=" << toUci(timed) << "\n";
    return 0;
}
