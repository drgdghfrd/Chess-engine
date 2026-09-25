#include "Bitboard.h"
#include <cstring>
namespace uc {
namespace BitOps {
// ---------------------------------------------------------------------------
// Knight / king / pawn attack tables (unchanged, already O(1) lookups)
// ---------------------------------------------------------------------------
static Bitboard KNIGHT_ATT[64];
static Bitboard KING_ATT[64];
static Bitboard PAWN_ATT[2][64];  // [white=1/black=0][sq]
static bool tablesReady = false;
static int rf(int sq) { return sq >> 3; }
static int ff(int sq) { return sq & 7; }
// ---------------------------------------------------------------------------
// Fancy magic bitboards for sliders. Standard constants (Berserk/Stockfish
// family) with the standard "relevant bits" masks that exclude outer edges.
// Total lookup memory: ~102400 + 5248 entries * 8 bytes = ~850 KB, and a
// lookup is one (occ & mask) * magic >> shift + table deref.
// ---------------------------------------------------------------------------
static constexpr int ROOK_TABLE_SIZE = 102400;
static constexpr int BISHOP_TABLE_SIZE = 5248;
static Bitboard ROOK_TABLE[ROOK_TABLE_SIZE];
static Bitboard BISHOP_TABLE[BISHOP_TABLE_SIZE];
struct Magic {
    Bitboard mask;
    uint64_t magic;
    int shift;
    Bitboard* attacks;
};
static Magic ROOK_MAGICS[64];
static Magic BISHOP_MAGICS[64];
static bool magicsReady = false;
static constexpr int ROOK_RELEVANT_BITS[64] = {
    12, 11, 11, 11, 11, 11, 11, 12,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    11, 10, 10, 10, 10, 10, 10, 11,
    12, 11, 11, 11, 11, 11, 11, 12
};
static constexpr int BISHOP_RELEVANT_BITS[64] = {
    6, 5, 5, 5, 5, 5, 5, 6,
    5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 7, 7, 7, 7, 5, 5,
    5, 5, 7, 9, 9, 7, 5, 5,
    5, 5, 7, 9, 9, 7, 5, 5,
    5, 5, 7, 7, 7, 7, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5,
    6, 5, 5, 5, 5, 5, 5, 6
};
static const uint64_t ROOK_MAGIC_NUMBERS[64] = {
    0x80800015c0082080ULL, 0x00c0100140002000ULL, 0x0100104009042000ULL, 0x0480080080100004ULL,
    0x1080040008008002ULL, 0x1200080410020001ULL, 0x030004ca00040500ULL, 0x408000a480004900ULL,
    0x0422800024904000ULL, 0x6000400050002001ULL, 0x1221002005021240ULL, 0x0000808008001000ULL,
    0x2050808004000800ULL, 0x0042000200080410ULL, 0x1004000241084410ULL, 0x080200040484690aULL,
    0x81c0808000284004ULL, 0x09c0018020008040ULL, 0x4000420020801200ULL, 0x4008008010000882ULL,
    0x8038008004008008ULL, 0x0802808002000400ULL, 0x0c10440021020890ULL, 0x0000020000840041ULL,
    0x0080005040002001ULL, 0x0000400480200080ULL, 0x0000104100200101ULL, 0x0010001080800800ULL,
    0x0000040080800800ULL, 0x2048020080040080ULL, 0x0880120400810850ULL, 0x028809020004884cULL,
    0x2440102040800086ULL, 0x1020100020404000ULL, 0x0861200184801000ULL, 0x0200801000800800ULL,
    0x0010080080800400ULL, 0x2202001002000409ULL, 0x04401022040008a1ULL, 0x0802049106000054ULL,
    0x0040804000228000ULL, 0x0050004020014010ULL, 0x2020200010008080ULL, 0x0010008100080800ULL,
    0x0028000400088080ULL, 0x4112010488020010ULL, 0x0462000408020001ULL, 0x10400ca841020004ULL,
    0x8006320146810200ULL, 0x0000812000400280ULL, 0x0010144020090100ULL, 0x008a001008452200ULL,
    0x0018004004020040ULL, 0x0020020004008080ULL, 0x0006011002080400ULL, 0x0000024700b40200ULL,
    0x0000410080002011ULL, 0x040811042182c001ULL, 0x58201041000a2001ULL, 0x0c00041001210009ULL,
    0x000200846010182aULL, 0x0001000208040013ULL, 0x9005000082002441ULL, 0x0484802102c40186ULL
};
static const uint64_t BISHOP_MAGIC_NUMBERS[64] = {
    0x0020828081010200ULL, 0x4020410421004045ULL, 0x4084080081030428ULL, 0x2002208200400040ULL,
    0x40240504102d0220ULL, 0x000a081424100200ULL, 0x0004108410080200ULL, 0x4040808050108400ULL,
    0x2004420822041042ULL, 0x0006101000890054ULL, 0x06085010c0810800ULL, 0x08000444008a080cULL,
    0x000a0d1041001000ULL, 0x1040008220600200ULL, 0x0010110110100400ULL, 0x00000830880c1040ULL,
    0x8840400510041108ULL, 0x0502000818510400ULL, 0x02411008080b0010ULL, 0x800406084400080eULL,
    0x4801004590400190ULL, 0x8101000080603200ULL, 0x0301110044100400ULL, 0x004020208a080200ULL,
    0x010844180aa01800ULL, 0x0904204004588880ULL, 0x1218510908020400ULL, 0x9008080040202120ULL,
    0x0120840202802000ULL, 0x5118024004806020ULL, 0x0942088684040120ULL, 0x0009010190440891ULL,
    0x3041101021882010ULL, 0x1000822040080801ULL, 0x0410280800010a00ULL, 0xc020400808038200ULL,
    0x0204200200402080ULL, 0x8090004200134100ULL, 0x8110010304204460ULL, 0x4021086200018a00ULL,
    0xc10808a208a01000ULL, 0x0024308818048410ULL, 0x4002010448004101ULL, 0x0402012011008802ULL,
    0x0000102012000041ULL, 0x00a1014101004200ULL, 0x0002820424008108ULL, 0xa210010069010880ULL,
    0x0800421011082208ULL, 0x8000804842102000ULL, 0x0400050088040015ULL, 0x0001020084043004ULL,
    0x02250c4010410040ULL, 0x200c910210010000ULL, 0x0a12029004108000ULL, 0x8028c84284014009ULL,
    0x00053c0200a2e000ULL, 0x1060102401080822ULL, 0x800404420082210dULL, 0x0100708002050412ULL,
    0x1100404240105100ULL, 0x08202120081042c0ULL, 0x0600204801082480ULL, 0x0a02a00202021220ULL
};
// Relevant-bits masks (outer edges excluded): used to compress occupancy.
static Bitboard rookMask(int sq) {
    Bitboard r = 0;
    int r0 = rf(sq), f0 = ff(sq);
    for (int rr = r0 + 1; rr <= 6; ++rr) r |= 1ULL << (rr * 8 + f0);
    for (int rr = r0 - 1; rr >= 1; --rr) r |= 1ULL << (rr * 8 + f0);
    for (int f = f0 + 1; f <= 6; ++f) r |= 1ULL << (r0 * 8 + f);
    for (int f = f0 - 1; f >= 1; --f) r |= 1ULL << (r0 * 8 + f);
    return r;
}
static Bitboard bishopMask(int sq) {
    Bitboard r = 0;
    int r0 = rf(sq), f0 = ff(sq);
    for (int rr = r0 + 1, f = f0 + 1; rr <= 6 && f <= 6; ++rr, ++f) r |= 1ULL << (rr * 8 + f);
    for (int rr = r0 + 1, f = f0 - 1; rr <= 6 && f >= 1; ++rr, --f) r |= 1ULL << (rr * 8 + f);
    for (int rr = r0 - 1, f = f0 + 1; rr >= 1 && f <= 6; --rr, ++f) r |= 1ULL << (rr * 8 + f);
    for (int rr = r0 - 1, f = f0 - 1; rr >= 1 && f >= 1; --rr, --f) r |= 1ULL << (rr * 8 + f);
    return r;
}
// On-the-fly slider attacks (naive loop): used ONLY to build the magic tables
// at startup. The hot path never calls these.
static Bitboard rookOTF(int sq, Bitboard occ) {
    Bitboard r = 0;
    int r0 = rf(sq), f0 = ff(sq);
    for (int d : {-1, 1}) {
        for (int f = f0 + d; f >= 0 && f < 8; f += d) {
            Bitboard b = 1ULL << (r0 * 8 + f); r |= b; if (occ & b) break;
        }
    }
    for (int d : {-1, 1}) {
        for (int rr = r0 + d; rr >= 0 && rr < 8; rr += d) {
            Bitboard b = 1ULL << (rr * 8 + f0); r |= b; if (occ & b) break;
        }
    }
    return r;
}
static Bitboard bishopOTF(int sq, Bitboard occ) {
    Bitboard r = 0;
    int r0 = rf(sq), f0 = ff(sq);
    static const int d[4][2] = {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
    for (auto& a : d) {
        int f = f0 + a[0], rr = r0 + a[1];
        while (f >= 0 && f < 8 && rr >= 0 && rr < 8) {
            Bitboard b = 1ULL << (rr * 8 + f); r |= b; if (occ & b) break;
            f += a[0]; rr += a[1];
        }
    }
    return r;
}
// Carry-Rippler: enumerate the i-th subset of `mask` with `bits` set bits.
static Bitboard setOccupancy(int idx, int bits, Bitboard mask) {
    Bitboard occ = 0;
    for (int i = 0; i < bits; ++i) {
        int j = lsb(mask);
        mask &= mask - 1;
        if (idx & (1 << i)) occ |= 1ULL << j;
    }
    return occ;
}
static void initMagics() {
    if (magicsReady) return;
    Bitboard* rt = ROOK_TABLE;
    for (int sq = 0; sq < 64; ++sq) {
        Magic& m = ROOK_MAGICS[sq];
        int bits = ROOK_RELEVANT_BITS[sq];
        int n = 1 << bits;
        m.mask = rookMask(sq);
        m.magic = ROOK_MAGIC_NUMBERS[sq];
        m.shift = 64 - bits;
        m.attacks = rt;
        for (int i = 0; i < n; ++i) {
            Bitboard occ = setOccupancy(i, bits, m.mask);
            rt[((occ & m.mask) * m.magic) >> m.shift] = rookOTF(sq, occ);
        }
        rt += n;
    }
    Bitboard* bt = BISHOP_TABLE;
    for (int sq = 0; sq < 64; ++sq) {
        Magic& m = BISHOP_MAGICS[sq];
        int bits = BISHOP_RELEVANT_BITS[sq];
        int n = 1 << bits;
        m.mask = bishopMask(sq);
        m.magic = BISHOP_MAGIC_NUMBERS[sq];
        m.shift = 64 - bits;
        m.attacks = bt;
        for (int i = 0; i < n; ++i) {
            Bitboard occ = setOccupancy(i, bits, m.mask);
            bt[((occ & m.mask) * m.magic) >> m.shift] = bishopOTF(sq, occ);
        }
        bt += n;
    }
    magicsReady = true;
}
void initAttackTables() {
    if (tablesReady) return;
    for (int sq = 0; sq < 64; ++sq) {
        int r0 = rf(sq), f0 = ff(sq);
        Bitboard kn = 0;
        static const int nd[8][2] = {
            {1, 2}, {2, 1}, {2, -1}, {1, -2},
            {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2}};
        for (auto& a : nd) {
            int f = f0 + a[0], r = r0 + a[1];
            if (f >= 0 && f < 8 && r >= 0 && r < 8)
                kn |= 1ULL << (r * 8 + f);
        }
        KNIGHT_ATT[sq] = kn;
        Bitboard kg = 0;
        for (int dr = -1; dr <= 1; ++dr)
            for (int df = -1; df <= 1; ++df)
                if (dr || df) {
                    int f = f0 + df, r = r0 + dr;
                    if (f >= 0 && f < 8 && r >= 0 && r < 8)
                        kg |= 1ULL << (r * 8 + f);
                }
        KING_ATT[sq] = kg;
        Bitboard pw = 0, pb = 0;
        if (r0 < 7) {
            if (f0 > 0) pw |= 1ULL << ((r0 + 1) * 8 + f0 - 1);
            if (f0 < 7) pw |= 1ULL << ((r0 + 1) * 8 + f0 + 1);
        }
        if (r0 > 0) {
            if (f0 > 0) pb |= 1ULL << ((r0 - 1) * 8 + f0 - 1);
            if (f0 < 7) pb |= 1ULL << ((r0 - 1) * 8 + f0 + 1);
        }
        PAWN_ATT[1][sq] = pw;
        PAWN_ATT[0][sq] = pb;
    }
    tablesReady = true;
    initMagics();
}
Bitboard knightAttacks(int sq) {
    if (!tablesReady) initAttackTables();
    return KNIGHT_ATT[sq];
}
Bitboard kingAttacks(int sq) {
    if (!tablesReady) initAttackTables();
    return KING_ATT[sq];
}
Bitboard knight(int sq) { return knightAttacks(sq); }
Bitboard king(int sq) { return kingAttacks(sq); }
Bitboard pawn(int sq, bool white) {
    if (!tablesReady) initAttackTables();
    return PAWN_ATT[white ? 1 : 0][sq];
}
Bitboard rook(int sq, Bitboard occ) {
    if (!magicsReady) initAttackTables();
    const Magic& m = ROOK_MAGICS[sq];
    return m.attacks[((occ & m.mask) * m.magic) >> m.shift];
}
Bitboard bishop(int sq, Bitboard occ) {
    if (!magicsReady) initAttackTables();
    const Magic& m = BISHOP_MAGICS[sq];
    return m.attacks[((occ & m.mask) * m.magic) >> m.shift];
}
Bitboard queen(int sq, Bitboard occ) { return rook(sq, occ) | bishop(sq, occ); }
Bitboard attackersTo(int sq, const Bitboards& bb, bool byWhite) {
    if (!magicsReady) initAttackTables();
    Bitboard occ = bb.white | bb.black, r = 0;
    int idx = byWhite ? 0 : 6;
    r |= pawn(sq, !byWhite) & bb.piece[idx + 0];
    r |= knight(sq) & bb.piece[idx + 1];
    r |= king(sq) & bb.piece[idx + 5];
    Bitboard diag = bishop(sq, occ);
    r |= diag & (bb.piece[idx + 2] | bb.piece[idx + 4]);
    Bitboard ortho = rook(sq, occ);
    r |= ortho & (bb.piece[idx + 3] | bb.piece[idx + 4]);
    return r;
}
}  // namespace BitOps
}  // namespace uc
