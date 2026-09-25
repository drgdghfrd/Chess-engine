#pragma once
#include <cstdint>

namespace uc {

// Zobrist keys: piece[pieceIndex 0..11][square 0..63], side, castling[16], ep[8]
struct Zobrist {
    static uint64_t piece[12][64];
    static uint64_t side;
    static uint64_t castling[16];
    static uint64_t ep[8];  // file a..h
    static bool ready;

    static void init();
};

// Map piece value (WP=1..WK=6, BP=-1..BK=-6) -> 0..11
inline int zobristPieceIndex(int pc) {
    if (pc > 0) return pc - 1;       // 0..5 white
    if (pc < 0) return 5 - pc;       // 6..11 black  (-1 -> 6, -6 -> 11)
    return -1;
}

}  // namespace uc
