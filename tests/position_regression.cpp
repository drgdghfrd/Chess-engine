#include "../src/chess/Position.h"
#include <iostream>
#include <algorithm>

using namespace uc;

int main() {
    Position p;
    if (!p.setFEN("4k3/8/8/8/8/8/4Q3/4K3 w - - 0 1")) {
        std::cerr << "failed to set test FEN\n";
        return 2;
    }

    const int e2 = p.parseSq("e2");
    const int e8 = p.parseSq("e8");
    Move captureKing{static_cast<uint8_t>(e2), static_cast<uint8_t>(e8), Flag::Capture};

    if (p.make(captureKing)) {
        std::cerr << "capturing the king was accepted\n";
        return 1;
    }

    const auto moves = p.legal();
    if (std::any_of(moves.begin(), moves.end(), [&](const Move& m) {
            return m.from == e2 && m.to == e8;
        })) {
        std::cerr << "legal move list contains king capture\n";
        return 1;
    }

    std::cout << "king-capture regression passed\n";
    return 0;
}
