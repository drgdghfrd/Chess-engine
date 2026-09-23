#include "../src/tablebase/Syzygy.h"
#include <cassert>
#include <iostream>

using namespace uc;

int main() {
    Syzygy tb;
    tb.setPath(".");
    tb.setProbeLimit(5);

    Position start;
    start.start();
    auto a = tb.status(start);
    assert(a.pieces == 32);
    assert(!a.eligible);
    assert(a.configured);
    assert(a.pathExists);
    assert(!a.probingAvailable);
    assert(a.probeLimit == 5);

    Position kqk;
    assert(kqk.setFEN("7k/8/8/8/8/8/6Q1/7K w - - 0 1"));
    auto b = tb.status(kqk);
    assert(b.pieces == 3);
    assert(b.eligible);
    assert(b.pathExists);
    assert(!b.probingAvailable);

    tb.setProbeLimit(99);
    assert(tb.probeLimit() == 7);
    tb.setProbeLimit(-4);
    assert(tb.probeLimit() == 0);
    assert(!tb.status(kqk).eligible);

    tb.setPath("__chesszero_missing_syzygy_path__");
    tb.setProbeLimit(5);
    auto c = tb.status(kqk);
    assert(c.configured);
    assert(!c.pathExists);
    assert(c.eligible);
    assert(!c.probingAvailable);

    std::cout << "v0.91 Syzygy integration tests: PASS\n"
              << "start_pieces=" << a.pieces
              << " kqk_eligible=" << (b.eligible ? 1 : 0)
              << " path_exists=" << (a.pathExists ? 1 : 0)
              << " probe_backend=" << (a.probingAvailable ? 1 : 0)
              << "\n";
    return 0;
}
