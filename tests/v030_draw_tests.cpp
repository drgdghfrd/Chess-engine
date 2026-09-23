#include "chess/Position.h"
#include "engine/Search.h"
#include <cassert>
#include <iostream>
#include <string>

using namespace uc;

static Move parseMove(Position& p, const std::string& uci) {
    assert(uci.size() >= 4);
    const int from = p.parseSq(uci.substr(0, 2));
    const int to   = p.parseSq(uci.substr(2, 2));
    for (const Move& m : p.legal()) {
        if (m.from != from || m.to != to) continue;
        if (!isPromotion(m) && uci.size() == 4) return m;
        if (isPromotion(m) && uci.size() >= 5) {
            const char c = uci[4];
            const int pp = promotionPiece(m);
            if ((pp == 2 && c == 'n') || (pp == 3 && c == 'b') ||
                (pp == 4 && c == 'r') || (pp == 5 && c == 'q'))
                return m;
        }
    }
    std::cerr << "Illegal test move: " << uci << " in " << p.fen() << '\n';
    assert(false);
    return {};
}

static void play(Position& p, const char* uci) {
    Move m = parseMove(p, uci);
    if (!p.make(m)) {
        std::cerr << "make() failed for test move: " << uci << "\n";
        std::abort();
    }
}

int main() {
    // Perft sanity: standard initial-position depth 3.
    Position perftPos;
    assert(perft(perftPos, 3) == 8902ULL);

    // Threefold: initial position occurs at ply 0, 4 and 8.
    Position rep;
    assert(rep.repetitionCount() == 1);
    const char* cycle[] = {
        "g1f3", "g8f6", "f3g1", "f6g8",
        "g1f3", "g8f6", "f3g1", "f6g8"
    };
    for (int i = 0; i < 8; ++i) play(rep, cycle[i]);
    assert(rep.repetitionCount() == 3);
    assert(rep.isDrawByRepetition());
    assert(rep.isDraw());
    assert(std::string(rep.drawReason()) == "repetition");

    // Search must stop immediately on a drawn root and return score 0.
    Search s(16);
    Move bm = s.go(rep, 8);
    (void)bm;
    assert(s.score() == 0);
    assert(s.nodes() == 0);

    // Null-move search plumbing must not count a synthetic null position.
    Position nullPos;
    assert(nullPos.repetitionCount() == 1);
    nullPos.makeNull();
    assert(nullPos.repetitionCount() == 1);
    nullPos.undoNull();
    assert(nullPos.repetitionCount() == 1);

    // FIDE 50-move halfmove threshold requested by v0.30 roadmap.
    Position fifty;
    assert(fifty.setFEN("8/8/8/8/8/8/4K3/7k w - - 100 1"));
    assert(fifty.halfmoveClock() == 100);
    assert(fifty.isDrawBy50Move());
    assert(fifty.isDraw());
    assert(std::string(fifty.drawReason()) == "50move");

    // Basic dead-material cases requested as optional v0.30 coverage.
    Position kvk;
    assert(kvk.setFEN("8/8/8/8/8/8/4K3/7k w - - 0 1"));
    assert(kvk.isInsufficientMaterial());

    Position kbk;
    assert(kbk.setFEN("8/8/8/8/8/8/2B1K3/7k w - - 0 1"));
    assert(kbk.isInsufficientMaterial());

    Position knk;
    assert(knk.setFEN("8/8/8/8/8/8/2N1K3/7k w - - 0 1"));
    assert(knk.isInsufficientMaterial());

    std::cout << "v0.30 draw tests: PASS\n";
    return 0;
}
