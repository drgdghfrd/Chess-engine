#include "../src/engine/TranspositionTable.h"
#include <cassert>
#include <iostream>

using namespace uc;

static Move m(int f, int t) {
    Move x{};
    x.from = static_cast<uint8_t>(f);
    x.to = static_cast<uint8_t>(t);
    x.flag = Flag::Quiet;
    return x;
}

int main() {
    TranspositionTable tt(1);
    tt.clear();
    tt.newSearch();

    int score = 0; Move best{}; bool found = false;
    const Move bm = m(12, 28);

    tt.store(0x12345678ULL, 8, 42, Bound::Exact, 3, bm);
    assert(tt.probe(0x12345678ULL, 8, -100, 100, 3, score, best, found));
    assert(score == 42 && found && best.from == 12 && best.to == 28);

    // A shallow request can still retrieve the exact score.
    assert(tt.probe(0x12345678ULL, 4, -100, 100, 3, score, best, found));
    assert(score == 42);

    // Lower/upper bounds respect the search window.
    tt.store(0xABCDEF01ULL, 10, 80, Bound::Lower, 0, bm);
    assert(tt.probe(0xABCDEF01ULL, 10, -20, 90, 0, score, best, found) == false);
    assert(tt.probe(0xABCDEF01ULL, 10, -20, 70, 0, score, best, found) == true);

    tt.store(0xABCDEF02ULL, 10, -80, Bound::Upper, 0, bm);
    assert(tt.probe(0xABCDEF02ULL, 10, -70, 20, 0, score, best, found) == true);

    const int hf = tt.hashfull();
    assert(hf >= 0 && hf <= 1000);

    std::cout << "v0.55 TT tests passed\n";
    return 0;
}
