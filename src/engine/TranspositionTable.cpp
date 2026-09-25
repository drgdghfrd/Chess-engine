#include "TranspositionTable.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <new>

namespace uc {

namespace {
struct OptionalMutexLock {
    std::mutex* m = nullptr;
    explicit OptionalMutexLock(std::mutex* mutex) : m(mutex) { if (m) m->lock(); }
    ~OptionalMutexLock() { if (m) m->unlock(); }
    OptionalMutexLock(const OptionalMutexLock&) = delete;
    OptionalMutexLock& operator=(const OptionalMutexLock&) = delete;
};
constexpr int MATE_SCORE = 29000;
constexpr int TB_SCORE = 28000;

int toTT(int score, int ply) {
    if (score >= TB_SCORE) return score + ply;
    if (score <= -TB_SCORE) return score - ply;
    return score;
}

int fromTT(int score, int ply) {
    if (score >= TB_SCORE) return score - ply;
    if (score <= -TB_SCORE) return score + ply;
    return score;
}

size_t nextPow2(size_t n) {
    if (n < 1) return 1;
    --n;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
#if SIZE_MAX > 0xffffffffu
    n |= n >> 32;
#endif
    return n + 1;
}

// 64-byte aligned allocation (cache line)
TTEntry* allocEntries(size_t n) {
    if (n == 0) return nullptr;
    const size_t bytes = n * sizeof(TTEntry);
#if defined(_ISOC11_SOURCE) || defined(__APPLE__) || defined(__linux__)
    void* p = nullptr;
    if (posix_memalign(&p, TranspositionTable::CACHE_LINE, bytes) != 0)
        return nullptr;
    std::memset(p, 0, bytes);
    // depth = -1 for empty: set explicitly
    auto* e = static_cast<TTEntry*>(p);
    for (size_t i = 0; i < n; ++i)
        e[i].depth = -1;
    return e;
#else
    auto* p = static_cast<TTEntry*>(std::malloc(bytes));
    if (!p) return nullptr;
    for (size_t i = 0; i < n; ++i)
        new (p + i) TTEntry{};
    return p;
#endif
}
}  // namespace

void TranspositionTable::freeTable() {
    if (table_) {
        std::free(table_);
        table_ = nullptr;
    }
    nEntries_ = 0;
    mask_ = 0;
}

TranspositionTable::TranspositionTable(size_t mb) {
    resize(mb);
}

TranspositionTable::~TranspositionTable() {
    freeTable();
}

void TranspositionTable::resize(size_t mb) {
    freeTable();
    size_t bytes = std::max<size_t>(1, mb) * 1024ULL * 1024ULL;
    size_t entries = bytes / sizeof(TTEntry);
    size_t buckets = nextPow2(std::max<size_t>(1, entries / CLUSTER));
    if (buckets > (1u << 24)) buckets = 1u << 24;
    mask_ = buckets - 1;
    nEntries_ = buckets * static_cast<size_t>(CLUSTER);
    table_ = allocEntries(nEntries_);
    age_ = 0;
}

void TranspositionTable::clear() {
    if (!table_) return;
    std::array<std::unique_lock<std::mutex>, STRIPES> locks;
    for (size_t i = 0; i < STRIPES; ++i) locks[i] = std::unique_lock<std::mutex>(stripes_[i]);
    for (size_t i = 0; i < nEntries_; ++i) {
        table_[i] = TTEntry{};
        table_[i].depth = -1;
    }
    age_ = 0;
}

void TranspositionTable::newSearch() {
    if (threadSafe_) {
        std::lock_guard<std::mutex> lock(stripes_[0]);
        age_ = static_cast<uint8_t>(age_ + 1);
        return;
    }
    age_ = static_cast<uint8_t>(age_ + 1);
}

bool TranspositionTable::probe(uint64_t key, int depth, int alpha, int beta,
                               int ply, int& score, Move& best,
                               bool& foundMove) const {
    foundMove = false;
    if (!table_ || nEntries_ == 0) return false;

    OptionalMutexLock lock(threadSafe_ ? &stripe(key) : nullptr);
    if (threadSafe_) concurrentOps_.fetch_add(1, std::memory_order_relaxed);
    size_t base = bucketIndex(key);
#if defined(__GNUC__)
    // Cluster = 128B = 2 cache lines, base is 128B-aligned if table_ is 64B-aligned
    // (bucket * 128): actually base index * 32; buckets power of 2 → cluster starts
    // at multiple of 128 bytes when base % 4 == 0 which it always is.
    const char* p = reinterpret_cast<const char*>(table_ + base);
    __builtin_prefetch(p, 0, 3);
    __builtin_prefetch(p + 64, 0, 3);
#endif

    const TTEntry* hit = nullptr;
    for (int i = 0; i < CLUSTER; ++i) {
        const TTEntry& e = table_[base + static_cast<size_t>(i)];
        if (e.key == key) {
            hit = &e;
            break;
        }
    }
    if (!hit) return false;

    best = hit->best;
    foundMove = (best.from != best.to || best.flag != Flag::Quiet || best.from != 0);
    int s = fromTT(hit->score, ply);
    score = s;
    if (hit->depth < depth) return false;

    auto bd = static_cast<Bound>(hit->bound);
    if (bd == Bound::Exact) return true;
    if (bd == Bound::Lower && s >= beta) return true;
    if (bd == Bound::Upper && s <= alpha) return true;
    return false;
}

void TranspositionTable::prefetch(uint64_t key) const {
    if (!table_) return;
    size_t base = bucketIndex(key);
#if defined(__GNUC__)
    const char* p = reinterpret_cast<const char*>(table_ + base);
    __builtin_prefetch(p, 0, 3);
    __builtin_prefetch(p + 64, 0, 3);
#endif
}


int TranspositionTable::hashfull() const {
    if (!table_ || nEntries_ == 0) return 0;

    // Sample up to 1000 entries. Counting current-generation entries gives
    // UCI-friendly HashFull semantics without scanning the whole TT.
    const size_t sample = std::min<size_t>(1000, nEntries_);
    size_t used = 0;
    for (size_t i = 0; i < sample; ++i) {
        std::lock_guard<std::mutex> lock(stripes_[i & (STRIPES - 1)]);
        const TTEntry& e = table_[i];
        if (e.key != 0 && e.depth >= 0 && e.age == age_)
            ++used;
    }
    return static_cast<int>((used * 1000) / sample);
}

void TranspositionTable::store(uint64_t key, int depth, int score, Bound bound,
                               int ply, const Move& best) {
    if (!table_ || nEntries_ == 0) return;

    OptionalMutexLock lock(threadSafe_ ? &stripe(key) : nullptr);
    if (threadSafe_) concurrentOps_.fetch_add(1, std::memory_order_relaxed);
    size_t base = bucketIndex(key);
#if defined(__GNUC__)
    char* p = reinterpret_cast<char*>(table_ + base);
    __builtin_prefetch(p, 1, 3);
    __builtin_prefetch(p + 64, 1, 3);
#endif

    TTEntry* empty = nullptr;
    TTEntry* worst = &table_[base];
    auto quality = [this](const TTEntry& e) {
        // Depth dominates; exact bounds and fresh generations are protected.
        // Older entries become progressively cheaper to replace.
        int q = e.depth * 2;
        if (e.bound == static_cast<uint8_t>(Bound::Exact)) q += 3;
        const uint8_t ageDelta = static_cast<uint8_t>(age_ - e.age);
        q += (ageDelta == 0 ? 8 : (ageDelta == 1 ? 2 : -2));
        return q;
    };

    for (int i = 0; i < CLUSTER; ++i) {
        TTEntry& e = table_[base + static_cast<size_t>(i)];
        if (e.key == key) {
            if (depth >= e.depth || bound == Bound::Exact || e.age != age_) {
                e.key = key;
                e.score = static_cast<int16_t>(toTT(score, ply));
                e.depth = static_cast<int16_t>(depth);
                e.bound = static_cast<uint8_t>(bound);
                e.age = age_;
                if (best.from || best.to || best.flag != Flag::Quiet)
                    e.best = best;
            } else if (best.from || best.to) {
                e.best = best;
                e.age = age_;
            }
            return;
        }
        if (e.depth < 0 || e.key == 0) {
            empty = &e;
            break;
        }
        if (quality(e) < quality(*worst))
            worst = &e;
    }

    TTEntry* dst = empty ? empty : worst;
    dst->key = key;
    dst->score = static_cast<int16_t>(toTT(score, ply));
    dst->depth = static_cast<int16_t>(depth);
    dst->bound = static_cast<uint8_t>(bound);
    dst->age = age_;
    dst->best = best;
}

}  // namespace uc
