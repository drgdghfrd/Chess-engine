#include "api/EngineAPI.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <thread>

int main() {
    cz::Engine e;

    if (!e.setPositionFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")) {
        std::cerr << "setPosition failed: " << e.lastError() << "\n";
        return 1;
    }

    if (!e.loadNetwork("nets/ChessZero-v0.75-halfkp.nnue") &&
        !e.loadNetwork("../nets/ChessZero-v0.75-halfkp.nnue")) {
        std::cerr << "network load failed: " << e.lastError() << "\n";
        return 2;
    }

    if (!e.setThreads(1) || !e.setHashMB(16)) {
        std::cerr << "configuration failed: " << e.lastError() << "\n";
        return 3;
    }

    const auto result = e.search(3, 0);
    if (!result.ok || result.bestmove[0] == '\0' || result.depth < 1 || result.nodes == 0) {
        std::cerr << "bad search result: move=" << result.bestmove
                  << " depth=" << result.depth << " nodes=" << result.nodes << "\n";
        return 4;
    }

    // Exercise the stop flag from a second thread. The exact completed depth is
    // intentionally not asserted; the API contract is responsiveness and a
    // deterministic return without races/crashes.
    bool apiReady = false;
    std::thread worker([&] {
        auto r = e.search(12, 0);
        apiReady = r.bestmove[0] != '\0';
    });
    for (int i = 0; i < 50 && !e.isSearching(); ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    e.stop();
    worker.join();
    if (!apiReady) {
        std::cerr << "stopped search returned no move\n";
        return 5;
    }

    CZEngine* raw = cz_engine_create();
    if (!raw) {
        std::cerr << "C ABI create failed\n";
        return 6;
    }
    char out[4096]{};
    if (!cz_engine_set_position_fen(raw, "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")) {
        std::cerr << "C ABI FEN failed: " << cz_engine_last_error(raw) << "\n";
        cz_engine_destroy(raw);
        return 7;
    }
    if (!cz_engine_search(raw, 2, 0, out, sizeof(out)) || std::string(out).find("bestmove|") != 0) {
        std::cerr << "C ABI search failed: " << out << "\n";
        cz_engine_destroy(raw);
        return 8;
    }
    cz_engine_destroy(raw);

    std::cout << "v104_engine_api: PASS bestmove=" << result.bestmove
              << " depth=" << result.depth << " nodes=" << result.nodes << "\n";
    return 0;
}
