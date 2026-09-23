#pragma once
#include <cstdint>
#include <array>

namespace uc {

using Bitboard = uint64_t;
struct Bitboards;

namespace BitOps {

inline int popcount(Bitboard x) {
#if defined(__ARM_NEON) || defined(USE_NEON)
    // scalar popcount still best for one word; NEON helps multi-board later
    return __builtin_popcountll(x);
#else
    return __builtin_popcountll(x);
#endif
}
inline int lsb(Bitboard x) { return x ? __builtin_ctzll(x) : -1; }

inline Bitboard north(Bitboard x) { return x << 8; }
inline Bitboard south(Bitboard x) { return x >> 8; }
inline Bitboard east(Bitboard x) { return (x & 0xfefefefefefefefeULL) << 1; }
inline Bitboard west(Bitboard x) { return (x & 0x7f7f7f7f7f7f7f7fULL) >> 1; }

// Precomputed attack tables (filled once)
void initAttackTables();
Bitboard knightAttacks(int sq);
Bitboard kingAttacks(int sq);

Bitboard knight(int sq);  // delegates to table after init
Bitboard king(int sq);
Bitboard pawn(int sq, bool white);
Bitboard rook(int sq, Bitboard occ);
Bitboard bishop(int sq, Bitboard occ);
Bitboard queen(int sq, Bitboard occ);
Bitboard attackersTo(int sq, const Bitboards& bb, bool byWhite);

}  // namespace BitOps

struct Bitboards {
    Bitboard white = 0, black = 0, piece[12]{};
};

}  // namespace uc
