#include "Zobrist.h"

namespace uc {

uint64_t Zobrist::piece[12][64];
uint64_t Zobrist::side;
uint64_t Zobrist::castling[16];
uint64_t Zobrist::ep[8];
bool Zobrist::ready = false;

// Deterministic PRNG (SplitMix64) — same keys every run
static uint64_t splitmix64(uint64_t& x) {
    uint64_t z = (x += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

void Zobrist::init() {
    if (ready) return;
    uint64_t seed = 0xC0FFEE5EEDULL;  // fixed seed
    for (int p = 0; p < 12; ++p)
        for (int s = 0; s < 64; ++s)
            piece[p][s] = splitmix64(seed);
    side = splitmix64(seed);
    for (int i = 0; i < 16; ++i)
        castling[i] = splitmix64(seed);
    for (int i = 0; i < 8; ++i)
        ep[i] = splitmix64(seed);
    ready = true;
}

}  // namespace uc
