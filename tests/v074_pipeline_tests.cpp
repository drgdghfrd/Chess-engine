#include "../src/nnue/NNUE.h"
#include "../src/chess/Position.h"
#include <cstdio>
#include <memory>
#include <string>
#include <sys/stat.h>

static bool existsAndLarge(const char* p) {
    struct stat st{};
    return ::stat(p, &st) == 0 && st.st_size > 10 * 1024 * 1024;
}

int main() {
    const char* candidates[] = {
        "nets/ChessZero-v0.74-sample-halfkp.nnue",
        "../nets/ChessZero-v0.74-sample-halfkp.nnue"
    };
    const char* path = nullptr;
    for (const char* p : candidates) {
        if (existsAndLarge(p)) { path = p; break; }
    }
    if (!path) {
        std::printf("v0.75 pipeline: FAIL sample HalfKP network not found\n");
        return 1;
    }

    // NNUE is ~10 MiB; keep it on the heap just like production search code.
    auto net = std::make_unique<uc::NNUE>();
    if (!net->load(path)) {
        std::printf("v0.75 pipeline: FAIL cannot load %s\n", path);
        return 1;
    }

    const char* fens[] = {
        "rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq - 0 1",
        "8/8/8/8/8/4K3/4Q3/4k3 w - - 0 1",
        "8/8/8/8/8/4K3/8/4k3 w - - 0 1"
    };
    for (const char* fen : fens) {
        uc::Position p;
        if (!p.setFEN(fen)) {
            std::printf("v0.75 pipeline: FAIL invalid test FEN\n");
            return 1;
        }
        const int score = net->evaluate(p);
        if (score < -30000 || score > 30000) {
            std::printf("v0.75 pipeline: FAIL score out of range: %d\n", score);
            return 1;
        }
    }

    std::printf("v0.75 pipeline: PASS CZNNUE32 HalfKP 40960x256 load + inference\n");
    return 0;
}
