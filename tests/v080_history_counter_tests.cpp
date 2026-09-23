#include "engine/Search.h"
#include "chess/Position.h"
#include "nnue/NNUE.h"
#include <cassert>
#include <iostream>

using namespace uc;

int main() {
    assert(network().load("../nets/ChessZero-v0.75-halfkp.nnue"));;
    Position p;
    p.setFEN("r1bq1rk1/ppp1bppp/2np1n2/8/2B1P3/2N1BN2/PPP2PPP/R2Q1RK1 w - - 0 1");

    Search s;
    assert(s.historyEnabled());
    assert(s.countermoveEnabled());
    assert(s.historyAggression() == 100);
    assert(s.countermoveAggression() == 100);

    Move m = s.go(p, 3);
    (void)m;
    assert(s.completedDepth() >= 1);
    assert(s.nodes() > 0);
    assert(s.historyUpdates() > 0);

    Search disabled;
    disabled.setHistory(false);
    disabled.setCountermove(false);
    assert(!disabled.historyEnabled());
    assert(!disabled.countermoveEnabled());
    Position p2;
    p2.setFEN("r1bq1rk1/ppp1bppp/2np1n2/8/2B1P3/2N1BN2/PPP2PPP/R2Q1RK1 w - - 0 1");
    Move m2 = disabled.go(p2, 3);
    (void)m2;
    assert(disabled.nodes() > 0);
    assert(disabled.historyUpdates() == 0);

    Search tuned;
    tuned.setHistoryAggression(150);
    tuned.setCountermoveAggression(75);
    assert(tuned.historyAggression() == 150);
    assert(tuned.countermoveAggression() == 75);
    Position p3;
    p3.setFEN("r1bq1rk1/ppp1bppp/2np1n2/8/2B1P3/2N1BN2/PPP2PPP/R2Q1RK1 w - - 0 1");
    Move m3 = tuned.go(p3, 3);
    (void)m3;
    assert(tuned.nodes() > 0);

    std::cout << "v0.80 History + Countermove tuning tests: PASS\n";
    std::cout << "history_updates=" << s.historyUpdates()
              << " countermove_hits=" << s.countermoveHits() << "\n";
    return 0;
}
