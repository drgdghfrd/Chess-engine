#include "../src/chess/Position.h"
#include "../src/engine/Search.h"
#include <cassert>
#include <iostream>
#include <string>

using namespace uc;

static bool samePosition(const Position& a, const Position& b) {
    return a.fen() == b.fen();
}

int main() {
    Position p, baseline;
    p.start();
    baseline.start();

    // RC guard: a bogus default Move must not alter the board.
    Move invalid{};
    assert(!p.make(invalid));
    assert(samePosition(p, baseline));

    Search s;
    Move m1 = s.go(p, 2);
    auto legal = p.legal();
    assert(!legal.empty());
    bool legal1 = false;
    for (const auto& m : legal)
        if (m.from == m1.from && m.to == m1.to && m.flag == m1.flag) legal1 = true;
    assert(legal1);

    // ucinewgame-equivalent reset: a fresh position must search independently
    // after the previous search has populated the TT.
    p.start();
    s.clearHash();
    Move m2 = s.go(p, 2);
    bool legal2 = false;
    for (const auto& m : legal)
        if (m.from == m2.from && m.to == m2.to && m.flag == m2.flag) legal2 = true;
    assert(legal2);

    // Stop-before-search must still produce a legal root move.
    s.stop();
    p.start();
    Move m3 = s.go(p, 8);
    legal2 = false;
    for (const auto& m : legal)
        if (m.from == m3.from && m.to == m3.to && m.flag == m3.flag) legal2 = true;
    assert(legal2);

    std::cout << "v0.99 RC engine guards: PASS\n"
              << "invalid_move_guard=true reset_safe=true legal_bestmoves=true\n";
}
