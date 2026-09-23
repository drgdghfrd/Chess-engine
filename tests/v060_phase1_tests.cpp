
// v0.60 Phase-1 smoke: search returns a legal-looking root move from startpos.
#include "chess/Position.h"
#include "engine/Search.h"
#include <cstdio>
#include <string>

int main() {
    uc::Position p;
    p.start();
    uc::Search s(8); // small TT
    uc::Move bm = s.go(p, 5);
    if (bm.from == bm.to && bm.flag == uc::Flag::Quiet && bm.from == 0) {
        // empty move only ok if no legal moves — startpos must move
        std::printf("v060_phase1: FAIL empty bestmove\n");
        return 1;
    }
    // must be on-board
    if (bm.from > 63 || bm.to > 63) {
        std::printf("v060_phase1: FAIL bad squares %d %d\n", (int)bm.from, (int)bm.to);
        return 1;
    }
    std::printf("v060_phase1: PASS bestmove from=%d to=%d nodes=%llu depth=%d\n",
                (int)bm.from, (int)bm.to,
                (unsigned long long)s.nodes(), s.completedDepth());
    return 0;
}
