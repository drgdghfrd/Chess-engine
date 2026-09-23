#include "Evaluation.h"
#include <iostream>
#include "../bitboard/Bitboard.h"

namespace uc {

static NNUE net;
static EvalMode g_evalMode = EvalMode::NNUE;

NNUE& network() { return net; }

namespace {

constexpr int MAT[7] = {0, 100, 320, 330, 500, 900, 20000};

constexpr int PST_P[64] = {
     0,  0,  0,  0,  0,  0,  0,  0,
     5, 10, 10,-20,-20, 10, 10,  5,
     5, -5,-10,  0,  0,-10, -5,  5,
     0,  0,  0, 20, 20,  0,  0,  0,
     5,  5, 10, 25, 25, 10,  5,  5,
    10, 10, 20, 30, 30, 20, 10, 10,
    50, 50, 50, 50, 50, 50, 50, 50,
     0,  0,  0,  0,  0,  0,  0,  0
};
constexpr int PST_N[64] = {
   -50,-40,-30,-30,-30,-30,-40,-50,
   -40,-20,  0,  0,  0,  0,-20,-40,
   -30,  0, 10, 15, 15, 10,  0,-30,
   -30,  5, 15, 20, 20, 15,  5,-30,
   -30,  0, 15, 20, 20, 15,  0,-30,
   -30,  5, 10, 15, 15, 10,  5,-30,
   -40,-20,  0,  5,  5,  0,-20,-40,
   -50,-40,-30,-30,-30,-30,-40,-50
};
constexpr int PST_B[64] = {
   -20,-10,-10,-10,-10,-10,-10,-20,
   -10,  5,  0,  0,  0,  0,  5,-10,
   -10, 10, 10, 10, 10, 10, 10,-10,
   -10,  0, 10, 10, 10, 10,  0,-10,
   -10,  5,  5, 10, 10,  5,  5,-10,
   -10,  0,  5, 10, 10,  5,  0,-10,
   -10,  0,  0,  0,  0,  0,  0,-10,
   -20,-10,-10,-10,-10,-10,-10,-20
};
constexpr int PST_R[64] = {
     0,  0,  0,  5,  5,  0,  0,  0,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
    -5,  0,  0,  0,  0,  0,  0, -5,
     5, 10, 10, 10, 10, 10, 10,  5,
     0,  0,  0,  0,  0,  0,  0,  0
};
constexpr int PST_Q[64] = {
   -20,-10,-10, -5, -5,-10,-10,-20,
   -10,  0,  0,  0,  0,  0,  0,-10,
   -10,  0,  5,  5,  5,  5,  0,-10,
    -5,  0,  5,  5,  5,  5,  0, -5,
     0,  0,  5,  5,  5,  5,  0, -5,
   -10,  5,  5,  5,  5,  5,  0,-10,
   -10,  0,  5,  0,  0,  0,  0,-10,
   -20,-10,-10, -5, -5,-10,-10,-20
};
constexpr int PST_K[64] = {
     20, 30, 10,  0,  0, 10, 30, 20,
     20, 20,  0,  0,  0,  0, 20, 20,
   -10,-20,-20,-20,-20,-20,-20,-10,
   -20,-30,-30,-40,-40,-30,-30,-20,
   -30,-40,-40,-50,-50,-40,-40,-30,
   -30,-40,-40,-50,-50,-40,-40,-30,
   -30,-40,-40,-50,-50,-40,-40,-30,
   -30,-40,-40,-50,-50,-40,-40,-30
};

inline const int* pstFor(int absPiece) {
    switch (absPiece) {
        case 1: return PST_P;
        case 2: return PST_N;
        case 3: return PST_B;
        case 4: return PST_R;
        case 5: return PST_Q;
        case 6: return PST_K;
        default: return PST_P;
    }
}

// ---- Pawn structure (v0.62) ----
constexpr Bitboard FILE_BB[8] = {
    0x0101010101010101ULL, 0x0202020202020202ULL, 0x0404040404040404ULL, 0x0808080808080808ULL,
    0x1010101010101010ULL, 0x2020202020202020ULL, 0x4040404040404040ULL, 0x8080808080808080ULL
};

inline Bitboard adjacentFiles(int f) {
    Bitboard a = 0;
    if (f > 0) a |= FILE_BB[f - 1];
    if (f < 7) a |= FILE_BB[f + 1];
    return a;
}

// Enemy-pawn span that can block a White passed pawn on `sq`
inline Bitboard whitePassersStop(int sq) {
    const int f = sq % 8;
    const int r = sq / 8;
    Bitboard stop = 0;
    for (int rr = r + 1; rr < 8; ++rr) {
        stop |= 1ULL << (rr * 8 + f);
        if (f > 0) stop |= 1ULL << (rr * 8 + f - 1);
        if (f < 7) stop |= 1ULL << (rr * 8 + f + 1);
    }
    return stop;
}

inline Bitboard blackPassersStop(int sq) {
    const int f = sq % 8;
    const int r = sq / 8;
    Bitboard stop = 0;
    for (int rr = r - 1; rr >= 0; --rr) {
        stop |= 1ULL << (rr * 8 + f);
        if (f > 0) stop |= 1ULL << (rr * 8 + f - 1);
        if (f < 7) stop |= 1ULL << (rr * 8 + f + 1);
    }
    return stop;
}

// Rank bonus for a passed pawn (from side's perspective: higher rank → more)
constexpr int PASSED_BONUS[8] = {0, 8, 15, 28, 45, 70, 110, 0};
constexpr int ISOLATED_PENALTY = 12;
constexpr int DOUBLED_PENALTY = 10;

// Returns score from White's point of view (positive = White better)
int evalPawnStructure(const Position& p) {
    const Bitboards& bb = p.bitboards();
    const Bitboard wp = bb.piece[0];
    const Bitboard bp = bb.piece[6];
    int s = 0;

    int wFile[8] = {}, bFile[8] = {};
    for (int f = 0; f < 8; ++f) {
        wFile[f] = BitOps::popcount(wp & FILE_BB[f]);
        bFile[f] = BitOps::popcount(bp & FILE_BB[f]);
    }

    Bitboard b = wp;
    while (b) {
        const int sq = BitOps::lsb(b);
        b &= b - 1;
        const int f = sq % 8;
        const int r = sq / 8;
        if ((wp & adjacentFiles(f)) == 0)
            s -= ISOLATED_PENALTY;
        if ((bp & whitePassersStop(sq)) == 0)
            s += PASSED_BONUS[r];
    }

    b = bp;
    while (b) {
        const int sq = BitOps::lsb(b);
        b &= b - 1;
        const int f = sq % 8;
        const int r = sq / 8;
        if ((bp & adjacentFiles(f)) == 0)
            s += ISOLATED_PENALTY;
        if ((wp & blackPassersStop(sq)) == 0)
            s -= PASSED_BONUS[7 - r];
    }

    for (int f = 0; f < 8; ++f) {
        if (wFile[f] > 1) s -= DOUBLED_PENALTY * (wFile[f] - 1);
        if (bFile[f] > 1) s += DOUBLED_PENALTY * (bFile[f] - 1);
    }

    return s;
}

// ---- King safety (v0.63): pawn shield + open files near king ----
constexpr int SHIELD_MISSING = 16;
constexpr int SHIELD_PRESENT = 4;
constexpr int KING_ADVANCED_PENALTY = 25;
constexpr int OPEN_FILE_NEAR_K = 10;

// White-POV terms for White's king
int evalKingShieldWhite(int ksq, Bitboard wp, Bitboard bp) {
    if (ksq < 0) return 0;
    const int f = ksq % 8;
    const int r = ksq / 8;
    int s = 0;
    if (r >= 3) s -= KING_ADVANCED_PENALTY;

    for (int df = -1; df <= 1; ++df) {
        const int ff = f + df;
        if (ff < 0 || ff > 7) continue;
        bool shield = false;
        for (int rr = r + 1; rr <= r + 2 && rr < 8; ++rr) {
            if (wp & (1ULL << (rr * 8 + ff))) { shield = true; break; }
        }
        s += shield ? SHIELD_PRESENT : -SHIELD_MISSING;
        if ((wp & FILE_BB[ff]) == 0) {
            s -= OPEN_FILE_NEAR_K;
            if ((bp & FILE_BB[ff]) == 0) s -= OPEN_FILE_NEAR_K / 2;
        }
    }
    return s;
}

// Returns White-POV score contribution from Black king safety
int evalKingShieldBlack(int ksq, Bitboard wp, Bitboard bp) {
    if (ksq < 0) return 0;
    const int f = ksq % 8;
    const int r = ksq / 8;
    int blackSafety = 0;
    // Black king advanced toward White (low ranks)
    if (r <= 4) blackSafety -= KING_ADVANCED_PENALTY;

    for (int df = -1; df <= 1; ++df) {
        const int ff = f + df;
        if (ff < 0 || ff > 7) continue;
        bool shield = false;
        for (int rr = r - 1; rr >= r - 2 && rr >= 0; --rr) {
            if (bp & (1ULL << (rr * 8 + ff))) { shield = true; break; }
        }
        blackSafety += shield ? SHIELD_PRESENT : -SHIELD_MISSING;
        if ((bp & FILE_BB[ff]) == 0) {
            blackSafety -= OPEN_FILE_NEAR_K;
            if ((wp & FILE_BB[ff]) == 0) blackSafety -= OPEN_FILE_NEAR_K / 2;
        }
    }
    return -blackSafety;
}

int evalKingSafety(const Position& p) {
    const Bitboards& bb = p.bitboards();
    const Bitboard wp = bb.piece[0];
    const Bitboard bp = bb.piece[6];
    return evalKingShieldWhite(p.kingSquare(true), wp, bp)
         + evalKingShieldBlack(p.kingSquare(false), wp, bp);
}



// ---- Mobility + center (v0.64) ----
constexpr Bitboard CENTER4 =
    (1ULL << 27) | (1ULL << 28) | (1ULL << 35) | (1ULL << 36);  // d4 e4 d5 e5
constexpr Bitboard CENTER16 =
    CENTER4
    | (1ULL << 18) | (1ULL << 19) | (1ULL << 20) | (1ULL << 21)  // c3-f3
    | (1ULL << 26) | (1ULL << 29)                                 // c4 f4
    | (1ULL << 34) | (1ULL << 37)                                 // c5 f5
    | (1ULL << 42) | (1ULL << 43) | (1ULL << 44) | (1ULL << 45); // c6-f6

constexpr int MOB_N = 4;
constexpr int MOB_B = 3;
constexpr int MOB_R = 2;
constexpr int MOB_Q = 1;
constexpr int CENTER_OCC = 8;   // own piece on d4/e4/d5/e5
constexpr int CENTER_EXT = 3;   // own piece on extended center

int countAttacks(Bitboard attacks, Bitboard own) {
    return BitOps::popcount(attacks & ~own);
}

int evalMobilityAndCenter(const Position& p) {
    const Bitboards& bb = p.bitboards();
    const Bitboard occ = bb.white | bb.black;
    int s = 0;

    // White pieces mobility + center occupation
    for (int sq = 0; sq < 64; ++sq) {
        const int pc = p.at(sq);
        if (!pc) continue;
        const Bitboard bit = 1ULL << sq;
        if (pc > 0) {
            if (bit & CENTER4) s += CENTER_OCC;
            else if (bit & CENTER16) s += CENTER_EXT;
            const int ab = pc;
            Bitboard att = 0;
            if (ab == WN) att = BitOps::knight(sq);
            else if (ab == WB) att = BitOps::bishop(sq, occ);
            else if (ab == WR) att = BitOps::rook(sq, occ);
            else if (ab == WQ) att = BitOps::queen(sq, occ);
            else continue;
            const int m = countAttacks(att, bb.white);
            if (ab == WN) s += m * MOB_N;
            else if (ab == WB) s += m * MOB_B;
            else if (ab == WR) s += m * MOB_R;
            else if (ab == WQ) s += m * MOB_Q;
        } else {
            if (bit & CENTER4) s -= CENTER_OCC;
            else if (bit & CENTER16) s -= CENTER_EXT;
            const int ab = -pc;
            Bitboard att = 0;
            if (ab == WN) att = BitOps::knight(sq);  // type index same abs
            else if (ab == WB) att = BitOps::bishop(sq, occ);
            else if (ab == WR) att = BitOps::rook(sq, occ);
            else if (ab == WQ) att = BitOps::queen(sq, occ);
            else continue;
            // abs pieces: WN=2 etc — use ab
            const int m = countAttacks(att, bb.black);
            if (ab == 2) s -= m * MOB_N;
            else if (ab == 3) s -= m * MOB_B;
            else if (ab == 4) s -= m * MOB_R;
            else if (ab == 5) s -= m * MOB_Q;
        }
    }
    return s;
}


// ---- Rook activity (v0.65) ----
constexpr int ROOK_OPEN_FILE = 20;
constexpr int ROOK_SEMI_OPEN = 10;
constexpr int ROOK_ON_7TH = 25;
constexpr int ROOK_CONNECTED = 8;  // same rank, no pieces between (approx: same rank both)

int evalRookActivity(const Position& p) {
    const Bitboards& bb = p.bitboards();
    const Bitboard wp = bb.piece[0];
    const Bitboard bp = bb.piece[6];
    const Bitboard wr = bb.piece[3];  // white rooks
    const Bitboard br = bb.piece[9];  // black rooks
    const Bitboard allPawns = wp | bp;
    int s = 0;

    Bitboard b = wr;
    while (b) {
        const int sq = BitOps::lsb(b);
        b &= b - 1;
        const int f = sq % 8;
        const int r = sq / 8;
        const Bitboard file = FILE_BB[f];
        if ((allPawns & file) == 0)
            s += ROOK_OPEN_FILE;
        else if ((wp & file) == 0)
            s += ROOK_SEMI_OPEN;
        if (r == 6)
            s += ROOK_ON_7TH;
    }

    b = br;
    while (b) {
        const int sq = BitOps::lsb(b);
        b &= b - 1;
        const int f = sq % 8;
        const int r = sq / 8;
        const Bitboard file = FILE_BB[f];
        if ((allPawns & file) == 0)
            s -= ROOK_OPEN_FILE;
        else if ((bp & file) == 0)
            s -= ROOK_SEMI_OPEN;
        if (r == 1)
            s -= ROOK_ON_7TH;
    }

    // Connected rooks on same rank (simple: ≥2 rooks, same rank bits)
    if (BitOps::popcount(wr) >= 2) {
        for (int r = 0; r < 8; ++r) {
            const Bitboard rankMask = 0xFFULL << (r * 8);
            if (BitOps::popcount(wr & rankMask) >= 2) {
                s += ROOK_CONNECTED;
                break;
            }
        }
    }
    if (BitOps::popcount(br) >= 2) {
        for (int r = 0; r < 8; ++r) {
            const Bitboard rankMask = 0xFFULL << (r * 8);
            if (BitOps::popcount(br & rankMask) >= 2) {
                s -= ROOK_CONNECTED;
                break;
            }
        }
    }

    return s;
}


// ---- Piece coordination (v0.66) ----
// Tuned v0.67: outpost is the main term; defended-only is smaller and
// does NOT stack on top of a full outpost (avoids 18+6 inflation).
constexpr int OUTPOST_N = 24;       // was 18 — strong stable knight
constexpr int OUTPOST_B = 10;       // was 12 — bishop outposts rarer/weaker
constexpr int MINOR_DEFENDED = 4;   // was 6 — only when not an outpost

// Pawn attack maps
inline Bitboard whitePawnAttacks(Bitboard wp) {
    return BitOps::east(BitOps::north(wp)) | BitOps::west(BitOps::north(wp));
}
inline Bitboard blackPawnAttacks(Bitboard bp) {
    return BitOps::east(BitOps::south(bp)) | BitOps::west(BitOps::south(bp));
}

int evalCoordination(const Position& p) {
    const Bitboards& bb = p.bitboards();
    const Bitboard wp = bb.piece[0];
    const Bitboard bp = bb.piece[6];
    const Bitboard wN = bb.piece[1];
    const Bitboard bN = bb.piece[7];
    const Bitboard wB = bb.piece[2];
    const Bitboard bB = bb.piece[8];
    const Bitboard wAttP = whitePawnAttacks(wp);
    const Bitboard bAttP = blackPawnAttacks(bp);
    const Bitboard occ = bb.white | bb.black;
    int s = 0;

    // White knights: outpost = on ranks 4–6, defended by own pawn, not attackable by enemy pawn
    Bitboard b = wN;
    while (b) {
        const int sq = BitOps::lsb(b);
        b &= b - 1;
        const int r = sq / 8;
        const Bitboard bit = 1ULL << sq;
        const bool defended = (wAttP & bit) != 0;
        const bool outpost = (r >= 3 && r <= 5) && defended && !(bAttP & bit);
        if (outpost)
            s += OUTPOST_N;
        else if (defended)
            s += MINOR_DEFENDED;
    }

    // Black knights
    b = bN;
    while (b) {
        const int sq = BitOps::lsb(b);
        b &= b - 1;
        const int r = sq / 8;
        const Bitboard bit = 1ULL << sq;
        const bool defended = (bAttP & bit) != 0;
        const bool outpost = (r >= 2 && r <= 4) && defended && !(wAttP & bit);
        if (outpost)
            s -= OUTPOST_N;
        else if (defended)
            s -= MINOR_DEFENDED;
    }

    // Bishops: outpost-like on ranks 3–5 / 2–4, pawn defended, not pawn-attacked
    b = wB;
    while (b) {
        const int sq = BitOps::lsb(b);
        b &= b - 1;
        const int r = sq / 8;
        const Bitboard bit = 1ULL << sq;
        const bool defended = (wAttP & bit) != 0;
        const bool outpost = (r >= 2 && r <= 5) && defended && !(bAttP & bit);
        if (outpost)
            s += OUTPOST_B;
        else if (defended)
            s += MINOR_DEFENDED / 2;
    }
    b = bB;
    while (b) {
        const int sq = BitOps::lsb(b);
        b &= b - 1;
        const int r = sq / 8;
        const Bitboard bit = 1ULL << sq;
        const bool defended = (bAttP & bit) != 0;
        const bool outpost = (r >= 2 && r <= 5) && defended && !(wAttP & bit);
        if (outpost)
            s -= OUTPOST_B;
        else if (defended)
            s -= MINOR_DEFENDED / 2;
    }

    // Rook behind passed pawn (simple): white rook on same file as white passer candidate
    // skipped — heavy; use rook on open already in v0.65

    (void)occ;
    return s;
}

}  // namespace

struct ClassicParts {
    int material = 0;
    int pst = 0;
    int bishopPair = 0;
    int pawns = 0;
    int king = 0;
    int mobility = 0;
    int rooks = 0;
    int coord = 0;
    int whitePov = 0;
    int stm = 0;
};

ClassicParts computeClassicParts(const Position& p) {
    ClassicParts c;
    int bishopsW = 0, bishopsB = 0;

    for (int sq = 0; sq < 64; ++sq) {
        const int pc = p.at(sq);
        if (!pc) continue;
        const int ab = pc > 0 ? pc : -pc;
        const int mat = MAT[ab];
        const int* pst = pstFor(ab);
        const int pstv = (pc > 0) ? pst[sq] : pst[sq ^ 56];

        if (pc > 0) {
            c.material += mat;
            c.pst += pstv;
            if (pc == WB) ++bishopsW;
        } else {
            c.material -= mat;
            c.pst -= pstv;
            if (pc == BB) ++bishopsB;
        }
    }

    if (bishopsW >= 2) c.bishopPair += 30;
    if (bishopsB >= 2) c.bishopPair -= 30;

    c.pawns = evalPawnStructure(p);
    c.king = evalKingSafety(p);
    c.mobility = evalMobilityAndCenter(p);
    c.rooks = evalRookActivity(p);
    c.coord = evalCoordination(p);

    c.whitePov = c.material + c.pst + c.bishopPair + c.pawns + c.king
               + c.mobility + c.rooks + c.coord;
    c.stm = (p.side() == 1) ? c.whitePov : -c.whitePov;
    return c;
}

int classicalEval(const Position& p) {
    return computeClassicParts(p).stm;
}

void printClassicalEval(const Position& p, std::ostream& os) {
    const ClassicParts c = computeClassicParts(p);
    os << "info string eval classical breakdown (cp, White POV unless noted)" << std::endl;
    os << "info string   material:     " << c.material << std::endl;
    os << "info string   pst:          " << c.pst << std::endl;
    os << "info string   bishop_pair:  " << c.bishopPair << std::endl;
    os << "info string   pawns:        " << c.pawns << std::endl;
    os << "info string   king_safety:  " << c.king << std::endl;
    os << "info string   mobility_ctr: " << c.mobility << std::endl;
    os << "info string   rooks:        " << c.rooks << std::endl;
    os << "info string   coordination: " << c.coord << std::endl;
    os << "info string   total_white:  " << c.whitePov << std::endl;
    os << "info string   total_stm:    " << c.stm
       << "  (side=" << (p.side() == 1 ? "w" : "b") << ")" << std::endl;
}


void setEvalMode(EvalMode mode) { g_evalMode = mode; }
EvalMode evalMode() { return g_evalMode; }

int evaluate(const Position& p) {
    if (g_evalMode == EvalMode::Classical) return classicalEval(p);
    return net.evaluate(p);
}

int evaluate(const Position& p, const NNUE::Accumulator& acc) {
    if (g_evalMode == EvalMode::Classical) return classicalEval(p);
    return net.evaluate(p, acc);
}

}  // namespace uc
