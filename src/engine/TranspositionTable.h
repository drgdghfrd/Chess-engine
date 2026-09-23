#pragma once
#include "../chess/Move.h"
#include <cstdint>
#include <cstddef>
#include <array>
#include <mutex>
#include <atomic>

namespace uc {

enum class Bound : uint8_t { Exact = 0, Lower = 1, Upper = 2 };

// 32-byte entry → CLUSTER of 4 = 128 bytes = exactly 2×64B cache lines.
// alignas(32) keeps entries from straddling 32B boundaries.
struct alignas(32) TTEntry {
    uint64_t key = 0;
    int16_t score = 0;
    int16_t depth = -1;
    uint8_t bound = 0;
    uint8_t age = 0;
    Move best{};
    uint8_t _pad[32 - 8 - 2 - 2 - 1 - 1 - 3]{};  // pad to 32 (Move ≈ 3 bytes)
};

static_assert(sizeof(TTEntry) == 32, "TTEntry must be 32 bytes for cache-line packing");
static_assert(alignof(TTEntry) == 32, "TTEntry align 32");

class TranspositionTable {
public:
    static constexpr int CLUSTER = 4;           // 4 * 32 = 128 = 2 cache lines
    static constexpr size_t CACHE_LINE = 64;

    explicit TranspositionTable(size_t mb = 32);
    ~TranspositionTable();

    TranspositionTable(const TranspositionTable&) = delete;
    TranspositionTable& operator=(const TranspositionTable&) = delete;

    void resize(size_t mb);
    void clear();
    void newSearch();

    bool probe(uint64_t key, int depth, int alpha, int beta, int ply,
               int& score, Move& best, bool& foundMove) const;

    void store(uint64_t key, int depth, int score, Bound bound, int ply,
               const Move& best);

    void prefetch(uint64_t key) const;
    void setThreadSafe(bool enabled) { threadSafe_ = enabled; }
    bool threadSafe() const { return threadSafe_; }

    size_t size() const { return nEntries_; }
    // Approximate occupancy in per-mille, sampled from the first entries of buckets.
    int hashfull() const;
    uint64_t concurrentOps() const { return concurrentOps_.load(std::memory_order_relaxed); }

private:
    TTEntry* table_ = nullptr;   // 64-byte-aligned base
    size_t nEntries_ = 0;
    size_t mask_ = 0;            // numBuckets - 1
    uint8_t age_ = 0;
    bool threadSafe_ = false;

    size_t bucketIndex(uint64_t key) const {
        return (static_cast<size_t>(key) & mask_) * static_cast<size_t>(CLUSTER);
    }

    void freeTable();
    std::mutex& stripe(uint64_t key) const { return stripes_[static_cast<size_t>(key) & (STRIPES - 1)]; }

    static constexpr size_t STRIPES = 256;
    mutable std::array<std::mutex, STRIPES> stripes_{};
    mutable std::atomic<uint64_t> concurrentOps_{0};
};

}  // namespace uc
