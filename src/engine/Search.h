#pragma once
#include "TranspositionTable.h"
#include "Evaluation.h"
#include "../chess/Position.h"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <string>
#include <thread>
#include <vector>
#include <memory>

namespace uc {

class Syzygy;

class Search {
public:
    explicit Search(size_t ttMB = 32);  // mobile: ~L3-aware default
    Search(size_t ttMB, TranspositionTable* sharedTT);
    Move go(Position&, int depth);
    Move go(Position&, int depth, int movetimeMs);
    Move goTimed(Position&, int maxDepth, int wtimeMs, int btimeMs,
                 int wincMs, int bincMs, int movesToGo = 0);
    void stop();
    void setExternalStop(std::atomic<bool>* flag) { externalStop_ = flag; }
    void setThreads(int n); // v1.1.0: runtime worker-pool configuration
    int threads() const { return hot_.threads; }
    uint64_t nodes() const { return nodes_.load(); }
    int score() const { return hot_.score; }
    int completedDepth() const { return hot_.completedDepth; }
    void setHashMB(size_t mb);
    size_t configuredHashMB() const { return configuredHashMB_; }
    size_t allocatedHashMB() const;
    size_t totalParallelHashMB() const;
    // v1.1.0: inspect the actual private-TT layout used by the active contexts.
    // Entry 0 is the root coordinator; the remaining entries are workers.
    std::vector<size_t> hashSlicesMB() const;
    size_t parallelWorkerCount() const { return parallelWorkers_.size(); }
    void clearHash();
    // v0.98: UCI time-management controls. MoveOverhead protects the clock
    // from GUI/OS/network latency; SlowMover scales the allocated thinking time.
    void setMoveOverheadMs(int ms) { hot_.moveOverheadMs = std::clamp(ms, 0, 1000); }
    int moveOverheadMs() const { return hot_.moveOverheadMs; }
    void setSlowMover(int pct) { hot_.slowMover = std::clamp(pct, 10, 1000); }
    int slowMover() const { return hot_.slowMover; }
    int budgetSoftMs() const { return hot_.budgetSoftMs; }
    int budgetHardMs() const { return hot_.budgetHardMs; }
    void setAspiration(bool enabled) { hot_.aspiration = enabled; }
    bool aspirationEnabled() const { return hot_.aspiration; }
    void setAspirationWindow(int cp) { hot_.aspirationWindow = std::clamp(cp, 4, 128); }
    int aspirationWindow() const { return hot_.aspirationWindow; }
    uint64_t aspirationSearches() const { return hot_.aspirationSearches; }
    uint64_t aspirationFailLows() const { return hot_.aspirationFailLows; }
    uint64_t aspirationFailHighs() const { return hot_.aspirationFailHighs; }
    uint64_t pvsResearches() const { return hot_.pvsResearches; }
    void setLMR(bool enabled) { hot_.lmr = enabled; }
    bool lmrEnabled() const { return hot_.lmr; }
    void setLMRAggression(int value) { hot_.lmrAggression = std::clamp(value, 50, 200); }
    int lmrAggression() const { return hot_.lmrAggression; }
    uint64_t lmrSearches() const { return hot_.lmrSearches; }
    uint64_t lmrResearches() const { return hot_.lmrResearches; }
    void setNullMove(bool enabled) { hot_.nullMove = enabled; }
    bool nullMoveEnabled() const { return hot_.nullMove; }
    void setNullMoveAggression(int value) { hot_.nullMoveAggression = std::clamp(value, 50, 200); }
    int nullMoveAggression() const { return hot_.nullMoveAggression; }
    void setNullMoveVerification(bool enabled) { hot_.nullMoveVerification = enabled; }
    bool nullMoveVerificationEnabled() const { return hot_.nullMoveVerification; }
    uint64_t nullMoveSearches() const { return hot_.nullMoveSearches; }
    uint64_t nullMoveCutoffs() const { return hot_.nullMoveCutoffs; }
    uint64_t nullMoveVerifications() const { return hot_.nullMoveVerifications; }
    uint64_t nullMoveVerificationFailures() const { return hot_.nullMoveVerificationFailures; }
    void setRazoring(bool enabled) { hot_.razoring = enabled; }
    bool razoringEnabled() const { return hot_.razoring; }
    void setRazoringAggression(int value) { hot_.razorAggression = std::clamp(value, 50, 200); }
    int razoringAggression() const { return hot_.razorAggression; }
    uint64_t razorSearches() const { return hot_.razorSearches; }
    uint64_t razorFailLows() const { return hot_.razorFailLows; }
    void setHistory(bool enabled) { hot_.historyEnabled = enabled; }
    bool historyEnabled() const { return hot_.historyEnabled; }
    void setHistoryAggression(int value) { hot_.historyAggression = std::clamp(value, 50, 200); }
    int historyAggression() const { return hot_.historyAggression; }
    void setCountermove(bool enabled) { hot_.countermove = enabled; }
    bool countermoveEnabled() const { return hot_.countermove; }
    void setCountermoveAggression(int value) { hot_.countermoveAggression = std::clamp(value, 50, 200); }
    int countermoveAggression() const { return hot_.countermoveAggression; }
    uint64_t historyUpdates() const { return hot_.historyUpdates; }
    uint64_t countermoveHits() const { return hot_.countermoveHits; }
    void setHistoryPruning(bool enabled) { hot_.historyPruning = enabled; }
    bool historyPruningEnabled() const { return hot_.historyPruning; }
    void setHistoryPruningAggression(int value) { hot_.historyPruningAggression = std::clamp(value, 50, 200); }
    int historyPruningAggression() const { return hot_.historyPruningAggression; }
    void setFutilityAggression(int value) { hot_.futilityAggression = std::clamp(value, 50, 200); }
    int futilityAggression() const { return hot_.futilityAggression; }
    uint64_t historyPrunes() const { return hot_.historyPrunes; }
    uint64_t futilityPrunes() const { return hot_.futilityPrunes; }
    uint64_t lazySmpLaunches() const { return hot_.lazySmpLaunches; }
    uint64_t lazySmpCompleted() const { return hot_.lazySmpCompleted; }
    uint64_t lazySmpAborted() const { return hot_.lazySmpAborted; }
    uint64_t parallelBestUpdates() const { return hot_.parallelBestUpdates; }
    uint64_t sharedTTOps() const { return tt().concurrentOps(); }

    std::string pvString() const;
    void setSyzygy(Syzygy* tb) { syzygy_ = tb; }
    Syzygy* syzygy() const { return syzygy_; }

private:
    static constexpr int MAX_PLY = 128;
    static constexpr int MAX_PV  = 64;   // PV line length (was MAX_PLY×MAX_PLY — L3 waste)
    static constexpr int MAX_HISTORY = 32768;

    Syzygy* syzygy_ = nullptr;
    TranspositionTable tt_;  // large; lives mostly outside L3
    TranspositionTable* sharedTT_ = nullptr;
    TranspositionTable& tt() { return sharedTT_ ? *sharedTT_ : tt_; }
    const TranspositionTable& tt() const { return sharedTT_ ? *sharedTT_ : tt_; }
    std::atomic<bool> stop_{false};
    std::atomic<bool>* externalStop_ = nullptr;
    // v1.1.0: Hash is a total memory budget for the whole configured search
    // context, not a multiplier per thread. The root coordinator and workers
    // receive private TT slices to avoid mutex contention in the search hot path.
    size_t configuredHashMB_ = 32;
    int appliedThreads_ = 1;
    size_t appliedHashMB_ = 32;

    // ---- Hot working set (aim L1/L2; keep contiguous) ----
    alignas(64) struct Hot {
        std::array<Move, MAX_PLY> killers1{};
        std::array<Move, MAX_PLY> killers2{};
        int history[2][64][64]{};
        Move counterMove[64][64]{};
        // v0.52: heuristic continuation history. The base table keeps the cheap previous-destination
        // context, while the piece-context table distinguishes the type of the previous mover.
        // int16 keeps both tables compact enough for mobile hot-state use.
        int16_t contHistory[64][64]{};
        int16_t contPieceHistory[6][64][64]{};
        // v0.50: bounded countermove history keyed by previous from/to and response to-square.
        int16_t counterHistory[64][64][64]{};
        Move pv[MAX_PLY][MAX_PV]{};
        int pvLen[MAX_PLY]{};
        NNUE::Accumulator rootAcc{};
        Move rootBest{};
        Move pvMove{};
        int score = 0;
        int completedDepth = 0;
        int threads = 1;
        bool timed = false;
        bool useSoft = false;
        // v0.98: clock-management controls and the last computed budget.
        int moveOverheadMs = 30;
        int slowMover = 100;
        int budgetSoftMs = 0;
        int budgetHardMs = 0;
        // v0.54: iterative-deepening stability state.
        Move lastCompletedBest{};
        int lastCompletedScore = 0;
        uint64_t lastDepthNodes = 0;
        uint64_t estimatedNextDepthNodes = 0;
        // v0.77: root aspiration/PVS tuning state.
        bool aspiration = true;
        int aspirationWindow = 20;
        uint64_t aspirationSearches = 0;
        uint64_t aspirationFailLows = 0;
        uint64_t aspirationFailHighs = 0;
        uint64_t pvsResearches = 0;
        // v0.78: configurable late-move reduction tuning. 100 = baseline.
        bool lmr = true;
        int lmrAggression = 100;
        uint64_t lmrSearches = 0;
        uint64_t lmrResearches = 0;
        // v0.79: configurable null-move pruning and razoring. 100 = baseline.
        bool nullMove = true;
        int nullMoveAggression = 100;
        bool nullMoveVerification = true;
        uint64_t nullMoveSearches = 0;
        uint64_t nullMoveCutoffs = 0;
        uint64_t nullMoveVerifications = 0;
        uint64_t nullMoveVerificationFailures = 0;
        bool razoring = true;
        int razorAggression = 100;
        uint64_t razorSearches = 0;
        uint64_t razorFailLows = 0;
        // v0.80: configurable history/countermove tuning. 100 = baseline.
        bool historyEnabled = true;
        int historyAggression = 100;
        bool countermove = true;
        int countermoveAggression = 100;
        uint64_t historyUpdates = 0;
        mutable uint64_t countermoveHits = 0;
        // v0.81: calibrated shallow history-pruning controls. 100 = baseline.
        bool historyPruning = true;
        int historyPruningAggression = 100;
        int futilityAggression = 100;
        uint64_t historyPrunes = 0;
        uint64_t futilityPrunes = 0;
        // v0.86: root-level Lazy SMP telemetry.
        uint64_t lazySmpLaunches = 0;
        uint64_t lazySmpCompleted = 0;
        // v0.88: parallel root coordination telemetry.
        uint64_t lazySmpAborted = 0;
        uint64_t parallelBestUpdates = 0;
    } hot_{};

    // Cold / infrequent vs search inner loop
    std::chrono::steady_clock::time_point hardDeadline_{};
    std::chrono::steady_clock::time_point softDeadline_{};
    std::chrono::steady_clock::time_point searchStart_{};
    std::atomic<uint64_t> nodes_{0};
    std::vector<Move> moveList_[MAX_PLY];
    // v1.0.5: persistent per-worker search contexts avoid constructing a new
    // TT and heuristic state for every root iteration. Private worker TTs
    // remove mutex traffic from the search hot path on mobile.
    std::vector<std::unique_ptr<Search>> parallelWorkers_;

    bool timeUp();          // hard limit / stop flag
    void applyParallelHashLayout();
    void prepareParallelWorkers();
    bool softTimeUp() const; // soft limit (checked between ID depths)
    uint64_t hash(const Position&) const;
    int pieceValue(int p) const;
    int moveScore(const Position&, const Move&, int ply, const Move& ttMove, const Move& prev) const;
    int qsearch(Position&, NNUE::Accumulator&, int, int, int);
    int negamax(Position&, NNUE::Accumulator&, int, int, int, int, const Move& prev, bool allowNull = true, const Move& excluded = Move{});
    int seeCapture(const Position&, const Move&) const;
    int rootSearch(Position&, NNUE::Accumulator&, int, int, int, Move&);
    int rootSearchParallel(Position&, NNUE::Accumulator&, int, int, int, Move&);
    void updateHistory(int side, const Move&, int depth, int bonusSign = 1);
    void updateContHistory(const Move& prev, const Move& m, int depth, int bonusSign = 1);
    void updateContPieceHistory(const Position&, const Move& prev, const Move& m, int depth, int bonusSign = 1);
    void updateCounterHistory(const Move& prev, const Move& m, int depth, int bonusSign = 1);
    int continuationScore(const Position&, const Move& prev, const Move& m) const;
    void updatePV(int ply, const Move& m);
    static std::vector<size_t> splitHashBudget(size_t totalMB, size_t contexts);
    // softMs / hardMs: 0 = unlimited
    Move searchInternal(Position&, int depth, int softMs, int hardMs);
};

}  // namespace uc
