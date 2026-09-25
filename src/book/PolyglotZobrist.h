#pragma once
#include "../chess/Position.h"
#include "PolyglotRandom.h"
#include <cstdint>
namespace uc {
class PolyglotZobrist {
public:
    static uint64_t key(const Position& p) {
        uint64_t h = 0;
        for (int sq = 0; sq < 64; ++sq) {
            const int pc = p.at(sq); if (!pc) continue;
            const int pt = pc < 0 ? -pc : pc;
            const int pivot = pc > 0 ? 1 : 0;
            h ^= POLYGLOT_RANDOM[64 * ((pt - 1) * 2 + pivot) + sq];
        }
        const int cr = p.castlingRights();
        if (cr & 1) h ^= POLYGLOT_RANDOM[768];
        if (cr & 2) h ^= POLYGLOT_RANDOM[769];
        if (cr & 4) h ^= POLYGLOT_RANDOM[770];
        if (cr & 8) h ^= POLYGLOT_RANDOM[771];
        const int ep = p.epSquare();
        if (ep >= 0) {
            const int f = ep & 7, r = ep >> 3; bool capturable = false;
            if (p.side() == 1) {
                const int fr = r - 1;
                if (fr >= 0) { if (f > 0 && p.at(fr*8+f-1) == WP) capturable = true; if (f < 7 && p.at(fr*8+f+1) == WP) capturable = true; }
            } else {
                const int fr = r + 1;
                if (fr < 8) { if (f > 0 && p.at(fr*8+f-1) == BP) capturable = true; if (f < 7 && p.at(fr*8+f+1) == BP) capturable = true; }
            }
            if (capturable) h ^= POLYGLOT_RANDOM[772 + f];
        }
        if (p.side() == 1) h ^= POLYGLOT_RANDOM[780];
        return h;
    }
    static bool isCanonicalTableReady() { return POLYGLOT_RANDOM.size() == 781; }
};
}
