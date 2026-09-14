#include "Evaluation.h"
#include <algorithm>
#include <cmath>

namespace uc {

static NNUE net;

NNUE& network() { return net; }

static int classicEval(const Position& p) {
    static const int v[7] = {0, 100, 320, 330, 500, 900, 20000};
    // Simple piece-square flavor: center bonus for knights/bishops/pawns.
    static const int center[64] = {
        0,0,0,0,0,0,0,0,
        0,2,4,6,6,4,2,0,
        0,4,8,10,10,8,4,0,
        0,6,10,14,14,10,6,0,
        0,6,10,14,14,10,6,0,
        0,4,8,10,10,8,4,0,
        0,2,4,6,6,4,2,0,
        0,0,0,0,0,0,0,0
    };
    int s = 0, bishopsW = 0, bishopsB = 0;
    for (int sq = 0; sq < 64; ++sq) {
        int pc = p.at(sq);
        if (!pc) continue;
        int ab = std::abs(pc);
        int val = v[ab];
        if (ab >= 1 && ab <= 3) val += center[sq];
        if (pc > 0) {
            s += val;
            if (pc == WB) ++bishopsW;
        } else {
            s -= val;
            if (pc == BB) ++bishopsB;
        }
    }
    if (bishopsW >= 2) s += 30;
    if (bishopsB >= 2) s -= 30;
    return p.side() == 1 ? s : -s;
}

int evaluate(const Position& p) {
    return net.loaded() ? net.evaluate(p) : classicEval(p);
}

int evaluate(const Position& p, const NNUE::Accumulator& acc) {
    return net.loaded() ? net.evaluate(p, acc) : classicEval(p);
}

}  // namespace uc
