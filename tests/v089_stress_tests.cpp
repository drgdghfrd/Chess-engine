#include "../src/engine/Search.h"
#include "../src/engine/Evaluation.h"
#include "../src/chess/Position.h"
#include <atomic>
#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>

using namespace uc;

int main() {
    assert(network().load("nets/ChessZero-v0.75-halfkp.nnue"));
    Position p; p.start();

    int stopped = 0;
    for (int round = 0; round < 3; ++round) {
        Search s(32);
        s.setThreads(4);
        Move result{};
        std::thread worker([&] { result = s.go(p, 10, 20); });
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
        s.stop();
        worker.join();
        assert(result.from < 64 && result.to < 64);
        ++stopped;
    }

    // A normal SMP search after repeated stops must still complete cleanly.
    Search normal(32);
    normal.setThreads(4);
    Move m = normal.go(p, 5);
    assert(m.from < 64 && m.to < 64);
    assert(normal.completedDepth() >= 1);
    assert(normal.lazySmpCompleted() > 0);

    std::cout << "v0.89 race/stress regression: PASS\n"
              << "parallel_rounds=3 stopped_searches=" << stopped
              << " final_completed=" << normal.lazySmpCompleted() << "\n";
    return 0;
}
