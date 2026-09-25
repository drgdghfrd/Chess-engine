#include "../src/engine/Search.h"
#include "../src/engine/Evaluation.h"
#include "../src/chess/Position.h"
#include <cassert>
#include <iostream>
#include <numeric>
#include <vector>

using namespace uc;

static size_t sum(const std::vector<size_t>& xs) {
    return std::accumulate(xs.begin(), xs.end(), size_t{0});
}

static void check_profile(size_t hashMB, int threads,
                          const std::vector<size_t>& expected) {
    Search s(32);
    s.setThreads(threads);
    s.setHashMB(hashMB);

    const auto slices = s.hashSlicesMB();
    assert(s.threads() == threads);
    assert(s.parallelWorkerCount() == static_cast<size_t>(threads - 1));
    assert(slices == expected);
    assert(sum(slices) == hashMB);
    assert(s.totalParallelHashMB() == hashMB);

    for (size_t mb : slices)
        assert(mb > 0 && "common low-memory profiles should give every active context a TT");
}

int main() {
    assert(network().load("nets/ChessZero-v0.75-halfkp.nnue"));
    Position p;
    p.start();

    // 7-thread Android MAX/PERFORMANCE low-memory profiles.
    check_profile(64, 7, {16, 8, 8, 8, 8, 8, 8});
    check_profile(32, 7, {8, 4, 4, 4, 4, 4, 4});
    check_profile(16, 7, {4, 2, 2, 2, 2, 2, 2});

    // 5-thread profiles: changing Threads re-layouts the same total budget.
    check_profile(64, 5, {16, 16, 16, 8, 8});
    check_profile(32, 5, {8, 8, 8, 4, 4});
    check_profile(16, 5, {4, 4, 4, 2, 2});

    // Search smoke test at the tightest common profile.
    Search runtime(32);
    runtime.setThreads(7);
    runtime.setHashMB(16);
    Move m = runtime.go(p, 4);
    assert(m.from < 64 && m.to < 64);
    assert(runtime.completedDepth() >= 1);

    std::cout << "v1.1.0 low-hash profiles: PASS\n"
              << "7T/64MB=" << runtime.hashSlicesMB().size() << " contexts tested\n"
              << "7T/16MB search bestmove=" << m.from << "-" << m.to << "\n";
    return 0;
}
