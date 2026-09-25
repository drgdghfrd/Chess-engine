#include <cassert>
#include <iostream>
#include "../src/chess/Position.h"

using namespace uc;

int main() {
    Position p;
    assert(p.setFEN("4k3/8/8/8/8/8/4R3/4K3 w - - 0 1"));
    Move captureKing{12, 60, Flag::Capture}; // e2 -> e8
    assert(!p.make(captureKing));

    // Position must remain unchanged after rejection.
    assert(p.side() == 1);
    assert(p.at(12) != 0);
    assert(p.at(60) != 0);

    // Ordinary legal move still works.
    Move quiet{12, 20, Flag::Quiet}; // e2 -> e3
    assert(p.make(quiet));
    p.undo();

    std::cout << "v1.0.2 position king-capture regression: PASS\n";
    return 0;
}
