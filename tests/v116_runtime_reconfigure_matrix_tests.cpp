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

static const int kThreads[] = {1, 2, 3, 4, 5, 6, 7};
static const size_t kHashes[] = {16, 32, 64, 128, 256};

int main() {
    assert(network().load("nets/ChessZero-v0.75-halfkp.nnue"));
    Position p;
    p.start();

    Search s(32);

    // Exhaustive configured-profile matrix for the Android/Chessis range.
    // Every requested Hash value is a TOTAL budget, independent of Threads.
    for (int threads : kThreads) {
        for (size_t hashMB : kHashes) {
            s.setThreads(threads);
            s.setHashMB(hashMB);

            assert(s.threads() == threads);
            assert(s.parallelWorkerCount() == static_cast<size_t>(threads - 1));
            assert(s.configuredHashMB() == hashMB);
            assert(s.totalParallelHashMB() == hashMB);

            const auto slices = s.hashSlicesMB();
            assert(slices.size() == static_cast<size_t>(threads));
            assert(sum(slices) == hashMB);
            for (size_t mb : slices)
                assert(mb > 0);
        }
    }

    // Walk Threads 1→7 while retaining each Hash setting.
    for (size_t hashMB : kHashes) {
        s.setHashMB(hashMB);
        for (int threads : kThreads) {
            s.setThreads(threads);
            assert(s.threads() == threads);
            assert(s.parallelWorkerCount() == static_cast<size_t>(threads - 1));
            assert(s.configuredHashMB() == hashMB);
            assert(s.totalParallelHashMB() == hashMB);
        }
    }

    // Walk Hash 16→256→16 while retaining every Threads setting.
    for (int threads : kThreads) {
        s.setThreads(threads);
        for (size_t hashMB : kHashes) {
            s.setHashMB(hashMB);
            assert(s.threads() == threads);
            assert(s.configuredHashMB() == hashMB);
            assert(s.parallelWorkerCount() == static_cast<size_t>(threads - 1));
            assert(s.totalParallelHashMB() == hashMB);
        }
        for (int i = static_cast<int>(sizeof(kHashes) / sizeof(kHashes[0])) - 1; i >= 0; --i) {
            const size_t hashMB = kHashes[i];
            s.setHashMB(hashMB);
            assert(s.threads() == threads);
            assert(s.configuredHashMB() == hashMB);
            assert(s.totalParallelHashMB() == hashMB);
        }
    }

    // Search smoke tests at every thread count using the smallest Hash profile.
    for (int threads : kThreads) {
        s.setThreads(threads);
        s.setHashMB(16);
        Move m = s.go(p, 3);
        assert(m.from < 64 && m.to < 64);
        assert(s.completedDepth() >= 1);
        assert(s.threads() == threads);
        assert(s.configuredHashMB() == 16);
        assert(s.totalParallelHashMB() == 16);
    }

    std::cout << "v1.1.0 runtime reconfigure matrix: PASS\n"
              << "thread_profiles=7 hash_profiles=5 combinations=35\n"
              << "thread_range=1..7 hash_range=16,32,64,128,256MB\n";
    return 0;
}
