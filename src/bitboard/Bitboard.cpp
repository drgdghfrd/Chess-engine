#include "Bitboard.h"

namespace uc {
namespace BitOps {

static Bitboard KNIGHT_ATT[64];
static Bitboard KING_ATT[64];
static Bitboard PAWN_ATT[2][64];  // [white=1/black=0][sq]
static bool tablesReady = false;

static int rf(int sq) { return sq >> 3; }
static int ff(int sq) { return sq & 7; }

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

        // Pawn attacks
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
    Bitboard r = 0;
    int r0 = rf(sq), f0 = ff(sq);
    for (int d : {-1, 1}) {
        for (int f = f0 + d; f >= 0 && f < 8; f += d) {
            Bitboard b = 1ULL << (r0 * 8 + f);
            r |= b;
            if (occ & b) break;
        }
    }
    for (int d : {-1, 1}) {
        for (int rr = r0 + d; rr >= 0 && rr < 8; rr += d) {
            Bitboard b = 1ULL << (rr * 8 + f0);
            r |= b;
            if (occ & b) break;
        }
    }
    return r;
}

Bitboard bishop(int sq, Bitboard occ) {
    Bitboard r = 0;
    int r0 = rf(sq), f0 = ff(sq);
    static const int d[4][2] = {{1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
    for (auto& a : d) {
        int f = f0 + a[0], rr = r0 + a[1];
        while (f >= 0 && f < 8 && rr >= 0 && rr < 8) {
            Bitboard b = 1ULL << (rr * 8 + f);
            r |= b;
            if (occ & b) break;
            f += a[0];
            rr += a[1];
        }
    }
    return r;
}

Bitboard queen(int sq, Bitboard occ) { return rook(sq, occ) | bishop(sq, occ); }

Bitboard attackersTo(int sq, const Bitboards& bb, bool byWhite) {
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
