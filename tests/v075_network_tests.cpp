#include "../src/nnue/NNUE.h"
#include "../src/chess/Position.h"
#include <cstdio>
#include <memory>
#include <sys/stat.h>

static bool existsLarge(const char* p) {
    struct stat st{};
    return ::stat(p, &st) == 0 && st.st_size > 10 * 1024 * 1024;
}

int main() {
    const char* candidates[] = {
        "nets/ChessZero-v0.75-halfkp.nnue",
        "../nets/ChessZero-v0.75-halfkp.nnue"
    };
    const char* path = nullptr;
    for (const char* p : candidates) if (existsLarge(p)) { path = p; break; }
    if (!path) { std::puts("v0.75 network: FAIL trained network not found"); return 1; }
    auto net = std::make_unique<uc::NNUE>();
    if (!net->load(path)) { std::puts("v0.75 network: FAIL load"); return 1; }
    const char* fens[] = {
        "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",
        "r1bqk2r/pppp1ppp/2n2n2/8/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 4 5",
        "8/8/8/8/8/4K3/4Q3/4k3 w - - 0 1"
    };
    for (const char* fen : fens) {
        uc::Position p;
        if (!p.setFEN(fen)) { std::puts("v0.75 network: FAIL FEN"); return 1; }
        int s = net->evaluate(p);
        if (s < -30000 || s > 30000) { std::puts("v0.75 network: FAIL score range"); return 1; }
    }
    std::puts("v0.75 network: PASS trained HalfKP 40960x256 load + inference");
    return 0;
}
