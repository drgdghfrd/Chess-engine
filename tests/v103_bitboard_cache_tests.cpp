#include "chess/Position.h"
#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

using namespace uc;

static Bitboards recompute(const Position& p) {
    Bitboards bb{};
    for (int sq = 0; sq < 64; ++sq) {
        const int pc = p.at(sq);
        if (!pc) continue;
        const int slot = pc > 0 ? pc - 1 : 6 + (-pc - 1);
        const Bitboard bit = Bitboard(1) << sq;
        bb.piece[slot] |= bit;
        if (pc > 0) bb.white |= bit;
        else bb.black |= bit;
    }
    return bb;
}

static bool same(const Bitboards& a, const Bitboards& b) {
    if (a.white != b.white || a.black != b.black) return false;
    for (int i = 0; i < 12; ++i)
        if (a.piece[i] != b.piece[i]) return false;
    return true;
}

static uint64_t xorshift64(uint64_t& x) {
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    return x;
}

int main() {
    Position p;
    p.start();
    uint64_t rng = 0x9e3779b97f4a7c15ULL;

    auto check = [&]() {
        const auto& cached = p.bitboards();
        const auto fresh = recompute(p);
        if (!same(cached, fresh)) {
            std::cerr << "cached bitboards diverged at FEN: " << p.fen() << '\n';
            return false;
        }
        return true;
    };

    for (int step = 0; step < 20000; ++step) {
        if (!check()) return 1;
        std::vector<Move> moves;
        p.generateLegal(moves);
        if (moves.empty()) break;
        const size_t idx = static_cast<size_t>(xorshift64(rng) % moves.size());
        if (!p.make(moves[idx])) return 2;
        if (!check()) return 3;

        if ((step % 7) == 0) {
            p.undo();
            if (!check()) return 4;
            if (!p.make(moves[idx])) return 5;
            if (!check()) return 6;
        }
    }

    while (true) {
        const std::string before = p.fen();
        std::vector<Move> moves;
        // There is no public history-size API; undo until the initial position
        // is restored, checking the cache after every operation.
        if (before == "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1") break;
        p.undo();
        if (!check()) return 7;
    }

    std::cout << "v1.0.3 bitboard cache: PASS\n";
    return 0;
}
