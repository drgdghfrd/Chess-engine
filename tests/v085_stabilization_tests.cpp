#include "../src/chess/Position.h"
#include "../src/engine/Search.h"
#include "../src/nnue/NNUE.h"
#include <cassert>
#include <iostream>
using namespace uc;

static bool legalMove(const Position& p, const Move& target) {
    for (const Move& m : p.legal()) if (m == target) return true;
    return false;
}

static void checkDefaults(Search& s) {
    assert(s.aspirationEnabled());
    assert(s.aspirationWindow() == 20);
    assert(s.lmrEnabled() && s.lmrAggression() == 100);
    assert(s.nullMoveEnabled() && s.nullMoveAggression() == 100);
    assert(s.nullMoveVerificationEnabled());
    assert(s.razoringEnabled() && s.razoringAggression() == 100);
    assert(s.historyEnabled() && s.historyAggression() == 100);
    assert(s.countermoveEnabled() && s.countermoveAggression() == 100);
    assert(s.historyPruningEnabled() && s.historyPruningAggression() == 100);
    assert(s.futilityAggression() == 100);
}

int main() {
    assert(network().load("nets/ChessZero-v0.75-halfkp.nnue"));

    // v0.85 locks the intended baseline: defaults remain conservative and
    // every tuning knob clamps to its documented safety range.
    Search baseline(32);
    checkDefaults(baseline);
    baseline.setLMRAggression(0); assert(baseline.lmrAggression() == 50);
    baseline.setLMRAggression(999); assert(baseline.lmrAggression() == 200);
    baseline.setNullMoveAggression(0); assert(baseline.nullMoveAggression() == 50);
    baseline.setNullMoveAggression(999); assert(baseline.nullMoveAggression() == 200);
    baseline.setRazoringAggression(0); assert(baseline.razoringAggression() == 50);
    baseline.setRazoringAggression(999); assert(baseline.razoringAggression() == 200);
    baseline.setHistoryAggression(0); assert(baseline.historyAggression() == 50);
    baseline.setHistoryAggression(999); assert(baseline.historyAggression() == 200);
    baseline.setCountermoveAggression(0); assert(baseline.countermoveAggression() == 50);
    baseline.setCountermoveAggression(999); assert(baseline.countermoveAggression() == 200);
    baseline.setHistoryPruningAggression(0); assert(baseline.historyPruningAggression() == 50);
    baseline.setHistoryPruningAggression(999); assert(baseline.historyPruningAggression() == 200);
    baseline.setFutilityAggression(0); assert(baseline.futilityAggression() == 50);
    baseline.setFutilityAggression(999); assert(baseline.futilityAggression() == 200);

    // Deterministic regression: two fresh searches on the same position must
    // produce the same root move/score at a fixed completed depth.
    Position p1; p1.start();
    Search a(32);
    Move ma = a.go(p1, 5);
    assert(legalMove(p1, ma));
    assert(a.completedDepth() >= 1);
    assert(a.nodes() > 0);

    Position p2; p2.start();
    Search b(32);
    Move mb = b.go(p2, 5);
    assert(legalMove(p2, mb));
    assert(b.completedDepth() == a.completedDepth());
    assert(mb == ma);
    assert(b.score() == a.score());

    // Feature toggles must not make the engine return an illegal root move.
    Search controls(32);
    controls.setLMR(false);
    controls.setNullMove(false);
    controls.setNullMoveVerification(false);
    controls.setRazoring(false);
    controls.setHistory(false);
    controls.setCountermove(false);
    controls.setHistoryPruning(false);
    Position p3; p3.start();
    Move mc = controls.go(p3, 4);
    assert(legalMove(p3, mc));
    assert(controls.completedDepth() >= 1);
    assert(controls.nodes() > 0);

    std::cout << "v0.85 stabilization tests: PASS\n";
    std::cout << "baseline_score=" << a.score()
              << " baseline_depth=" << a.completedDepth()
              << " baseline_nodes=" << a.nodes() << "\n";
    return 0;
}
