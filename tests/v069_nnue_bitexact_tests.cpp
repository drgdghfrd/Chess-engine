// v0.69 — bit-exact NNUE: incremental applyMoveFeatures vs full refresh
#include "nnue/NNUE.h"
#include "chess/Position.h"
#include "chess/Move.h"
#include "engine/Evaluation.h"
#include <cstdio>
#include <cstring>
#include <vector>

using uc::NNUE;
using uc::Position;
using uc::Move;
using uc::Flag;
using uc::isCapture;

static bool accEqual(const NNUE::Accumulator& a, const NNUE::Accumulator& b) {
    return std::memcmp(a.data, b.data, sizeof(a.data)) == 0;
}

static int runSequence(NNUE& net, const std::vector<Move>& seq, const char* name) {
    Position p;
    p.start();
    NNUE::Accumulator inc{}, ref{};
    net.refresh(p, inc);

    for (size_t i = 0; i < seq.size(); ++i) {
        const Move m = seq[i];
        const int moved = p.at(m.from);
        int captured = isCapture(m) ? p.at(m.to) : 0;
        if (m.flag == Flag::EnPassant)
            captured = p.at(m.to + (moved > 0 ? -8 : 8));

        if (!p.make(m)) {
            std::printf("v069: FAIL %s make[%zu]\n", name, i);
            return 1;
        }
        const int toPiece = p.at(m.to);
        const bool ep = (m.flag == Flag::EnPassant);
        net.applyMoveFeatures(p, inc, m, moved, captured, ep, toPiece, +1);

        net.refresh(p, ref);
        if (!accEqual(inc, ref)) {
            std::printf("v069: FAIL %s after make[%zu] incremental != refresh\n", name, i);
            return 1;
        }
    }

    // Undo path: reverse features then undo, match refresh of parent
    for (int i = static_cast<int>(seq.size()) - 1; i >= 0; --i) {
        // Rebuild truth from current board via refresh before undo of move i
        // Current board is after moves 0..i inclusive; we need to undo move i.
        // Capture the move from history by re-playing is hard — instead:
        // verify: from current, we can't easily get move params without hist.
        break;
    }

    std::printf("v069: PASS %s forward incremental (%zu plies)\n", name, seq.size());
    return 0;
}

// Replay with undo bit-exact: store move metadata
struct Step {
    Move m;
    int moved;
    int captured;
    int toPiece;
    bool ep;
};

static int runWithUndo(NNUE& net, const std::vector<Move>& seq, const char* name) {
    Position p;
    p.start();
    NNUE::Accumulator inc{};
    net.refresh(p, inc);
    std::vector<Step> steps;

    for (size_t i = 0; i < seq.size(); ++i) {
        Step st;
        st.m = seq[i];
        st.moved = p.at(st.m.from);
        st.captured = isCapture(st.m) ? p.at(st.m.to) : 0;
        if (st.m.flag == Flag::EnPassant)
            st.captured = p.at(st.m.to + (st.moved > 0 ? -8 : 8));
        if (!p.make(st.m)) {
            std::printf("v069: FAIL %s make[%zu]\n", name, i);
            return 1;
        }
        st.toPiece = p.at(st.m.to);
        st.ep = (st.m.flag == Flag::EnPassant);
        net.applyMoveFeatures(p, inc, st.m, st.moved, st.captured, st.ep, st.toPiece, +1);
        steps.push_back(st);

        NNUE::Accumulator ref{};
        net.refresh(p, ref);
        if (!accEqual(inc, ref)) {
            std::printf("v069: FAIL %s fwd[%zu] inc!=refresh\n", name, i);
            return 1;
        }
    }

    for (int i = static_cast<int>(steps.size()) - 1; i >= 0; --i) {
        const Step& st = steps[static_cast<size_t>(i)];
        net.applyMoveFeatures(p, inc, st.m, st.moved, st.captured, st.ep, st.toPiece, -1);
        p.undo();

        NNUE::Accumulator ref{};
        net.refresh(p, ref);
        if (!accEqual(inc, ref)) {
            std::printf("v069: FAIL %s undo[%d] inc!=refresh\n", name, i);
            return 1;
        }
    }

    std::printf("v069: PASS %s forward+undo (%zu plies)\n", name, seq.size());
    return 0;
}

static Move U(int from, int to, Flag fl = Flag::Quiet) {
    return Move{static_cast<uint8_t>(from), static_cast<uint8_t>(to), fl};
}

int main() {
    NNUE& net = uc::network();
    // try load demo
    const char* paths[] = {
        "nets/ChessZero-v0.32-demo.nnue",
        "../nets/ChessZero-v0.32-demo.nnue",
        "ChessZero-v0.32-demo.nnue"
    };
    bool ok = false;
    for (const char* pth : paths) {
        if (net.load(pth)) { ok = true; break; }
    }
    if (!ok) {
        std::printf("v069: SKIP (no NNUE file — put demo net in nets/)\n");
        return 0;
    }

    // startpos: e2e4 e7e5 g1f3
    // squares: e2=12 e4=28 e7=52 e5=36 g1=6 f3=21 (a1=0)
    std::vector<Move> seq1 = {
        U(12, 28, Flag::DoublePawn),
        U(52, 36, Flag::DoublePawn),
        U(6, 21)
    };

    if (runWithUndo(net, seq1, "open")) return 1;

    // longer: + d2d4 d7d5
    std::vector<Move> seq2 = {
        U(12, 28, Flag::DoublePawn),
        U(52, 36, Flag::DoublePawn),
        U(6, 21),
        U(51, 35, Flag::DoublePawn), // d7d5
        U(11, 27, Flag::DoublePawn)  // d2d4
    };
    if (runWithUndo(net, seq2, "open5")) return 1;

    std::printf("v069: 100%% PASS bit-exact incremental vs refresh\n");
    return 0;
}
