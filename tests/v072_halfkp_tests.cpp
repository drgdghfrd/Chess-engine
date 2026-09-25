// v0.72 — HalfKP dedicated test: king-move incremental must be bit-exact
// with full refresh. This is the hardest case because every feature of
// the moving side is king-anchored and must be recomputed from scratch.
#include "chess/Position.h"
#include "chess/Move.h"
#include "nnue/NNUE.h"
#include <cassert>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>

using namespace uc;

// Create a synthetic HalfKP net (40960 inputs, 256 hidden) for testing.
static void writeHalfKpNet(const char* path) {
    struct H {
        char magic[8];
        uint32_t version, inputs, hidden, output, shift, reserved;
    };
    H h{{'C','Z','N','N','U','E','3','2'},
        32, NNUE::INPUTS, NNUE::HIDDEN_SIZE, 1, 8, 0};
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(&h), sizeof(h));
    for (uint32_t i = 0; i < NNUE::HIDDEN_SIZE; ++i) {
        int16_t b = static_cast<int16_t>((i % 13) - 6);
        f.write(reinterpret_cast<char*>(&b), 2);
    }
    for (uint32_t i = 0; i < NNUE::INPUTS * NNUE::HIDDEN_SIZE; ++i) {
        int8_t w = static_cast<int8_t>((i % 11) - 5);
        f.write(reinterpret_cast<char*>(&w), 1);
    }
    for (uint32_t i = 0; i < NNUE::HIDDEN_SIZE; ++i) {
        int8_t w = static_cast<int8_t>((i % 7) - 3);
        f.write(reinterpret_cast<char*>(&w), 1);
    }
    int32_t ob = 23;
    f.write(reinterpret_cast<char*>(&ob), 4);
}

static bool accEqual(const NNUE::Accumulator& a, const NNUE::Accumulator& b) {
    if (!a.valid || !b.valid) return false;
    if (a.whiteKingSq != b.whiteKingSq) return false;
    if (a.blackKingSq != b.blackKingSq) return false;
    return std::memcmp(a.data, b.data, sizeof(a.data)) == 0;
}

// Helper: make a move directly from square names
static Move makeMove(const char* from, const char* to, Flag fl = Flag::Quiet) {
    int f = (from[0] - 'a') + (from[1] - '1') * 8;
    int t = (to[0] - 'a') + (to[1] - '1') * 8;
    return Move{static_cast<uint8_t>(f), static_cast<uint8_t>(t), fl};
}

static int testNormalMoves(NNUE& net) {
    Position p;  // startpos
    NNUE::Accumulator inc{}, ref{};
    net.refresh(p, inc);

    const char* seq[][3] = {
        {"e2", "e4", "DP"}, {"e7", "e5", "DP"},
        {"g1", "f3", "Q"},  {"b8", "c6", "Q"},
        {"f1", "b5", "Q"},  {"a7", "a6", "Q"},
        {"b5", "a4", "Q"},  {"g8", "f6", "Q"},
        {"d2", "d3", "Q"},  {"h7", "h6", "Q"},
        {"c2", "c3", "Q"},  {"f8", "c5", "Q"}
    };
    const int N = sizeof(seq) / sizeof(seq[0]);
    for (int i = 0; i < N; ++i) {
        Flag fl = (seq[i][2][0] == 'D') ? Flag::DoublePawn : Flag::Quiet;
        Move m = makeMove(seq[i][0], seq[i][1], fl);
        int moved = p.at(m.from);
        int captured = isCapture(m) ? p.at(m.to) : 0;
        bool ep = (m.flag == Flag::EnPassant);
        if (ep) captured = p.at(m.to + (moved > 0 ? -8 : 8));

        bool ok = p.make(m);
        if (!ok) {
            std::printf("v0.72 HalfKP: FAIL could not make move %s%s at step %d\n",
                        seq[i][0], seq[i][1], i);
            return 1;
        }
        net.applyMoveFeatures(p, inc, m, moved, captured, ep, p.at(m.to), +1);
        net.refresh(p, ref);

        if (!accEqual(inc, ref)) {
            std::printf("v0.72 HalfKP: FAIL normal move %s%s: inc != refresh\n",
                        seq[i][0], seq[i][1]);
            return 1;
        }
    }
    std::printf("v0.72 HalfKP: PASS normal incremental (%d plies)\n", N);
    return 0;
}

static int testKingMoves(NNUE& net) {
    // White king moves
    {
        Position p;
        // Play e2e4, e7e5 so white king has e2 square available
        p.make(makeMove("e2", "e4", Flag::DoublePawn));
        p.make(makeMove("e7", "e5", Flag::DoublePawn));
        // Now side = white (1)

        NNUE::Accumulator inc{}, ref{};
        net.refresh(p, inc);

        // White king moves: e1 -> e2
        Move m = makeMove("e1", "e2");
        int moved = p.at(m.from);
        int captured = 0;
        bool ok = p.make(m);
        if (!ok) {
            std::printf("v0.72 HalfKP: FAIL could not make white king e1e2\n");
            return 1;
        }
        net.applyMoveFeatures(p, inc, m, moved, captured, false, p.at(m.to), +1);
        net.refresh(p, ref);
        if (!accEqual(inc, ref)) {
            std::printf("v0.72 HalfKP: FAIL white king e1e2: inc != refresh\n");
            return 1;
        }
        std::printf("v0.72 HalfKP: PASS white king move (e1e2)\n");

        // Undo king move
        net.applyMoveFeatures(p, inc, m, moved, captured, false, p.at(m.to), -1);
        p.undo();
        net.refresh(p, ref);
        if (!accEqual(inc, ref)) {
            std::printf("v0.72 HalfKP: FAIL white king undo: inc != refresh\n");
            return 1;
        }
        std::printf("v0.72 HalfKP: PASS white king move undo\n");
    }

    // Black king moves
    {
        Position p;
        p.make(makeMove("e2", "e4", Flag::DoublePawn));
        p.make(makeMove("e7", "e6", Flag::DoublePawn));
        // side = white (1), but we want black to move — play one more white move
        p.make(makeMove("g1", "f3"));
        // Now side = black (-1)

        NNUE::Accumulator inc{}, ref{};
        net.refresh(p, inc);

        // Black king moves: e8 -> e7
        Move m = makeMove("e8", "e7");
        int moved = p.at(m.from);
        int captured = 0;
        bool ok = p.make(m);
        if (!ok) {
            std::printf("v0.72 HalfKP: FAIL could not make black king e8e7\n");
            return 1;
        }
        net.applyMoveFeatures(p, inc, m, moved, captured, false, p.at(m.to), +1);
        net.refresh(p, ref);
        if (!accEqual(inc, ref)) {
            std::printf("v0.72 HalfKP: FAIL black king e8e7: inc != refresh\n");
            return 1;
        }
        std::printf("v0.72 HalfKP: PASS black king move (e8e7)\n");
    }

    return 0;
}

static int testCastling(NNUE& net) {
    Position p;
    if (!p.setFEN("r3k2r/pppppppp/8/8/8/8/PPPPPPPP/R3K2R w KQkq - 0 1")) {
        std::printf("v0.72 HalfKP: FAIL setFEN for castling\n");
        return 1;
    }
    NNUE::Accumulator inc{}, ref{};
    net.refresh(p, inc);

    Move m = makeMove("e1", "g1", Flag::KingCastle);
    int moved = p.at(m.from);
    int captured = 0;
    bool ok = p.make(m);
    if (!ok) {
        std::printf("v0.72 HalfKP: FAIL could not make kingside castling\n");
        return 1;
    }
    net.applyMoveFeatures(p, inc, m, moved, captured, false, p.at(m.to), +1);
    net.refresh(p, ref);
    if (!accEqual(inc, ref)) {
        std::printf("v0.72 HalfKP: FAIL white kingside castling: inc != refresh\n");
        return 1;
    }
    std::printf("v0.72 HalfKP: PASS white kingside castling\n");

    // Undo castling
    net.applyMoveFeatures(p, inc, m, moved, captured, false, p.at(m.to), -1);
    p.undo();
    net.refresh(p, ref);
    if (!accEqual(inc, ref)) {
        std::printf("v0.72 HalfKP: FAIL castling undo: inc != refresh\n");
        return 1;
    }
    std::printf("v0.72 HalfKP: PASS castling undo\n");

    return 0;
}

static int testEnPassantAndPromotion(NNUE& net) {
    // En passant
    {
        Position p;
        if (!p.setFEN("4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1")) {
            std::printf("v0.72 HalfKP: FAIL setFEN for en-passant\n");
            return 1;
        }
        NNUE::Accumulator inc{}, ref{};
        net.refresh(p, inc);
        Move m = makeMove("e5", "d6", Flag::EnPassant);
        int moved = p.at(m.from);
        int captured = p.at(m.to + (moved > 0 ? -8 : 8));
        bool ok = p.make(m);
        if (!ok) {
            std::printf("v0.72 HalfKP: FAIL could not make en-passant\n");
            return 1;
        }
        net.applyMoveFeatures(p, inc, m, moved, captured, true, p.at(m.to), +1);
        net.refresh(p, ref);
        if (!accEqual(inc, ref)) {
            std::printf("v0.72 HalfKP: FAIL en-passant: inc != refresh\n");
            return 1;
        }
        std::printf("v0.72 HalfKP: PASS en-passant\n");
    }

    // Promotion
    {
        Position p;
        if (!p.setFEN("4k3/P7/8/8/8/8/8/4K3 w - - 0 1")) {
            std::printf("v0.72 HalfKP: FAIL setFEN for promotion\n");
            return 1;
        }
        NNUE::Accumulator inc{}, ref{};
        net.refresh(p, inc);
        Move m = makeMove("a7", "a8", Flag::PromoQueen);
        int moved = p.at(m.from);
        int captured = 0;
        bool ok = p.make(m);
        if (!ok) {
            std::printf("v0.72 HalfKP: FAIL could not make promotion\n");
            return 1;
        }
        net.applyMoveFeatures(p, inc, m, moved, captured, false, p.at(m.to), +1);
        net.refresh(p, ref);
        if (!accEqual(inc, ref)) {
            std::printf("v0.72 HalfKP: FAIL promotion: inc != refresh\n");
            return 1;
        }
        std::printf("v0.72 HalfKP: PASS promotion\n");
    }

    return 0;
}

static int testKingSqTracking(NNUE& net) {
    Position p;
    p.make(makeMove("e2", "e4", Flag::DoublePawn));
    p.make(makeMove("e7", "e5", Flag::DoublePawn));
    // side = white

    NNUE::Accumulator beforeKingMove{};
    net.refresh(p, beforeKingMove);

    // Save black side BEFORE white king moves
    int16_t blackBefore[NNUE::HIDDEN_SIZE];
    std::memcpy(blackBefore, beforeKingMove.black(), sizeof(blackBefore));
    int blackKingSqBefore = beforeKingMove.blackKingSq;

    // Move white king
    Move m = makeMove("e1", "e2");
    int moved = p.at(m.from);
    bool ok = p.make(m);
    if (!ok) {
        std::printf("v0.72 HalfKP: FAIL could not make king move for tracking test\n");
        return 1;
    }

    NNUE::Accumulator afterKingMove = beforeKingMove;
    net.applyMoveFeatures(p, afterKingMove, m, moved, 0, false, p.at(m.to), +1);

    // Black side must be IDENTICAL (black king didn't move)
    if (std::memcmp(blackBefore, afterKingMove.black(), sizeof(blackBefore)) != 0) {
        std::printf("v0.72 HalfKP: FAIL black side corrupted by white king move\n");
        return 1;
    }
    if (afterKingMove.blackKingSq != blackKingSqBefore) {
        std::printf("v0.72 HalfKP: FAIL black king square changed\n");
        return 1;
    }
    // White king square must be updated
    if (afterKingMove.whiteKingSq != m.to) {
        std::printf("v0.72 HalfKP: FAIL white king square not updated (got %d, expected %d)\n",
                    afterKingMove.whiteKingSq, m.to);
        return 1;
    }
    std::printf("v0.72 HalfKP: PASS king-square tracking (other side untouched)\n");
    return 0;
}

int main() {
    const char* netPath = "v072_halfkp_test.nnue";
    writeHalfKpNet(netPath);

    // Use heap allocation: NNUE contains a 10MB weights array that would
    // overflow default stack size (~8MB).
    auto netPtr = std::make_unique<NNUE>();
    NNUE& net = *netPtr;

    if (!net.load(netPath)) {
        std::printf("v0.72 HalfKP: FAIL — could not load synthetic HalfKP net\n");
        std::remove(netPath);
        return 1;
    }
    assert(net.loaded());
    std::printf("v0.72 HalfKP: synthetic net loaded OK (inputs=%d, hidden=%d)\n",
                NNUE::INPUTS, NNUE::HIDDEN_SIZE);

    int failures = 0;
    failures += testNormalMoves(net);
    failures += testKingMoves(net);
    failures += testCastling(net);
    failures += testEnPassantAndPromotion(net);
    failures += testKingSqTracking(net);

    std::remove(netPath);

    if (failures == 0) {
        std::printf("\nv0.72 HalfKP: ALL TESTS PASSED — incremental is bit-exact with refresh\n");
        return 0;
    }
    std::printf("\nv0.72 HalfKP: %d test(s) FAILED\n", failures);
    return 1;
}
