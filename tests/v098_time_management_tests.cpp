#include "../src/engine/Search.h"
#include "../src/chess/Position.h"
#include <cassert>
#include <chrono>
#include <iostream>

using namespace uc;

int main(){
    Position p; p.start();
    Search s(4);

    // Baseline clock: the computed hard budget must leave explicit overhead
    // plus the safety reserve outside the search allocation.
    s.setMoveOverheadMs(100);
    s.setSlowMover(100);
    auto t0=std::chrono::steady_clock::now();
    Move m=s.goTimed(p, 8, 5000, 5000, 0, 0, 20);
    auto elapsed=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-t0).count();
    assert(m.from < 64 && m.to < 64);
    assert(s.budgetSoftMs() >= 1);
    assert(s.budgetHardMs() >= s.budgetSoftMs());
    assert(s.budgetHardMs() <= 4900);
    assert(elapsed < 4900);

    // SlowMover should increase the requested soft budget when there is room,
    // while the hard safety cap remains intact.
    Search slow(4);
    slow.setMoveOverheadMs(0);
    slow.setSlowMover(200);
    slow.goTimed(p, 8, 10000, 10000, 0, 0, 20);
    const int slowBudget=slow.budgetSoftMs();
    Search normal(4);
    normal.setMoveOverheadMs(0);
    normal.setSlowMover(100);
    normal.goTimed(p, 8, 10000, 10000, 0, 0, 20);
    assert(slowBudget >= normal.budgetSoftMs());
    assert(slow.budgetHardMs() >= slow.budgetSoftMs());

    // Emergency clock: allocation must remain tiny and finite.
    Search emergency(4);
    emergency.setMoveOverheadMs(30);
    emergency.goTimed(p, 4, 120, 120, 0, 0, 1);
    assert(emergency.budgetHardMs() >= emergency.budgetSoftMs());
    assert(emergency.budgetHardMs() < 120);

    std::cout << "v0.98 time-management budgets: PASS\n"
              << "baseline_soft=" << s.budgetSoftMs() << " baseline_hard=" << s.budgetHardMs()
              << " slow_soft=" << slowBudget << " emergency_hard=" << emergency.budgetHardMs() << "\n";
}
