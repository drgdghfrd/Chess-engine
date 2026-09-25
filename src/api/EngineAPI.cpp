#include "EngineAPI.h"
#include "../engine/Search.h"
#include "../engine/Evaluation.h"
#include "../nnue/NNUE.h"
#include "../chess/Position.h"
#include "../chess/Move.h"
#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <exception>
#include <mutex>
#include <string>
namespace cz {
class Engine::Impl {
public:
    uc::Position position;
    uc::Search search{32};
    mutable std::mutex stateMutex;
    std::atomic<bool> searching{false};
    std::atomic<bool> externalStop{false};
    std::condition_variable idleCv;
    mutable std::mutex idleMutex;
    std::string error;
    Impl() {
        search.setExternalStop(&externalStop);
        uc::network().loadEmbedded();
    }
    void setError(const char* msg) {
        std::lock_guard<std::mutex> lock(stateMutex);
        error = msg ? msg : "unknown error";
    }
};
Engine::Engine() : impl_(new Impl()) {
    impl_->position.start();
}
Engine::~Engine() {
    stop();
    waitIdle();
    delete impl_;
}
bool Engine::setPositionFEN(const char* fen) {
    if (!fen || !*fen) {
        impl_->setError("empty FEN");
        return false;
    }
    if (impl_->searching.load(std::memory_order_acquire)) {
        impl_->setError("engine busy");
        return false;
    }
    uc::Position next;
    if (!next.setFEN(fen)) {
        impl_->setError("invalid FEN");
        return false;
    }
    std::lock_guard<std::mutex> lock(impl_->stateMutex);
    impl_->position = std::move(next);
    impl_->error.clear();
    return true;
}
bool Engine::newGame() {
    if (impl_->searching.load(std::memory_order_acquire)) {
        impl_->setError("engine busy");
        return false;
    }
    std::lock_guard<std::mutex> lock(impl_->stateMutex);
    impl_->position.start();
    impl_->error.clear();
    return true;
}
bool Engine::makeMoveUCI(const char* uci) {
    if (!uci || std::strlen(uci) < 4) {
        impl_->setError("invalid move");
        return false;
    }
    if (impl_->searching.load(std::memory_order_acquire)) {
        impl_->setError("engine busy");
        return false;
    }
    const std::string text(uci);
    const std::string fromName = text.substr(0, 2);
    const std::string toName = text.substr(2, 2);
    const char promotion = text.size() >= 5 ? text[4] : '\0';
    std::lock_guard<std::mutex> lock(impl_->stateMutex);
    const int from = impl_->position.parseSq(fromName);
    const int to = impl_->position.parseSq(toName);
    if (from < 0 || from >= 64 || to < 0 || to >= 64) {
        impl_->error = "invalid move square";
        return false;
    }
    for (const uc::Move& move : impl_->position.legal()) {
        if (move.from != from || move.to != to) continue;
        const bool promo = uc::isPromotion(move);
        if (promo) {
            int wanted = 0;
            switch (promotion) {
            case 'q': wanted = 5; break;
            case 'r': wanted = 4; break;
            case 'b': wanted = 3; break;
            case 'n': wanted = 2; break;
            default: continue;
            }
            if (uc::promotionPiece(move) != wanted) continue;
        } else if (promotion != '\0') {
            continue;
        }
        if (!impl_->position.make(move)) break;
        impl_->error.clear();
        return true;
    }
    impl_->error = "illegal move";
    return false;
}
bool Engine::undoMove() {
    if (impl_->searching.load(std::memory_order_acquire)) {
        impl_->setError("engine busy");
        return false;
    }
    std::lock_guard<std::mutex> lock(impl_->stateMutex);
    impl_->position.undo();
    impl_->error.clear();
    return true;
}
std::string Engine::currentFEN() const {
    std::lock_guard<std::mutex> lock(impl_->stateMutex);
    return impl_->position.fen();
}
bool Engine::loadNetwork(const char* path) {
    if (!path || !*path) {
        impl_->setError("empty network path");
        return false;
    }
    if (impl_->searching.load(std::memory_order_acquire)) {
        impl_->setError("engine busy");
        return false;
    }
    if (!uc::network().load(path)) {
        impl_->setError("failed to load NNUE network");
        return false;
    }
    impl_->setError("");
    return true;
}
bool Engine::nnueLoaded() const noexcept {
    return uc::network().loaded();
}
bool Engine::setThreads(int threads) {
    // Runtime resource changes are safe only after the active search and all
    // of its worker threads have quiesced. Stop-and-wait makes Android API
    // callers behave the same way as the UCI path: the new setting applies to
    // the next search instead of being rejected as "busy".
    if (impl_->searching.load(std::memory_order_acquire)) {
        impl_->externalStop.store(true, std::memory_order_release);
        impl_->search.stop();
        waitIdle();
    }
    impl_->search.setThreads(std::clamp(threads, 1, 64));
    impl_->setError("");
    return true;
}
bool Engine::setHashMB(std::size_t mb) {
    if (impl_->searching.load(std::memory_order_acquire)) {
        impl_->externalStop.store(true, std::memory_order_release);
        impl_->search.stop();
        waitIdle();
    }
    impl_->search.setHashMB(std::clamp<std::size_t>(mb, 1, 2048));
    impl_->setError("");
    return true;
}
bool Engine::clearHash() {
    if (impl_->searching.load(std::memory_order_acquire)) {
        impl_->setError("engine busy");
        return false;
    }
    impl_->search.clearHash();
    impl_->setError("");
    return true;
}
SearchResult Engine::search(int depth, int movetimeMs) {
    SearchResult result;
    if (impl_->searching.exchange(true, std::memory_order_acq_rel)) {
        impl_->setError("engine busy");
        return result;
    }
    // Separate external cancellation from Search's internal per-search stop flag.
    // This prevents a stop arriving during the tiny startup window from being
    // lost when Search::searchInternal() resets its own stop flag.
    impl_->externalStop.store(false, std::memory_order_release);
    uc::Position local;
    {
        std::lock_guard<std::mutex> lock(impl_->stateMutex);
        local = impl_->position;
        impl_->error.clear();
    }
    try {
        uc::Move best;
        if (movetimeMs > 0)
            best = impl_->search.go(local, std::clamp(depth, 1, 64), movetimeMs);
        else
            best = impl_->search.go(local, std::clamp(depth, 1, 64));
        result.ok = (best.from < 64 && best.to < 64);
        result.depth = impl_->search.completedDepth();
        result.score = impl_->search.score();
        result.nodes = impl_->search.nodes();
        if (result.ok) {
            const std::string uci = uc::toUci(best);
            std::snprintf(result.bestmove, sizeof(result.bestmove), "%s", uci.c_str());
            std::snprintf(result.pv, sizeof(result.pv), "%s", impl_->search.pvString().c_str());
        }
    } catch (const std::exception& ex) {
        impl_->setError(ex.what());
        result = SearchResult{};
    } catch (...) {
        impl_->setError("unknown native search exception");
        result = SearchResult{};
    }
    impl_->searching.store(false, std::memory_order_release);
    impl_->idleCv.notify_all();
    return result;
}
void Engine::stop() {
    impl_->externalStop.store(true, std::memory_order_release);
    impl_->search.stop();
}
void Engine::waitIdle() {
    std::unique_lock<std::mutex> lock(impl_->idleMutex);
    impl_->idleCv.wait(lock, [this] {
        return !impl_->searching.load(std::memory_order_acquire);
    });
}
bool Engine::isSearching() const noexcept {
    return impl_->searching.load(std::memory_order_acquire);
}
const char* Engine::lastError() const noexcept {
    std::lock_guard<std::mutex> lock(impl_->stateMutex);
    return impl_->error.c_str();
}
} // namespace cz
struct CZEngine {
    cz::Engine impl;
};
extern "C" {
CZEngine* cz_engine_create() {
    try {
        return new CZEngine();
    } catch (...) {
        return nullptr;
    }
}
void cz_engine_destroy(CZEngine* engine) {
    delete engine;
}
int cz_engine_set_position_fen(CZEngine* engine, const char* fen) {
    return engine && engine->impl.setPositionFEN(fen) ? 1 : 0;
}
int cz_engine_new_game(CZEngine* engine) {
    return engine && engine->impl.newGame() ? 1 : 0;
}
int cz_engine_make_move_uci(CZEngine* engine, const char* uci) {
    return engine && engine->impl.makeMoveUCI(uci) ? 1 : 0;
}
int cz_engine_undo_move(CZEngine* engine) {
    return engine && engine->impl.undoMove() ? 1 : 0;
}
int cz_engine_get_fen(const CZEngine* engine, char* out, std::size_t outSize) {
    if (!engine || !out || outSize == 0) return 0;
    const std::string fen = engine->impl.currentFEN();
    if (fen.size() + 1 > outSize) return 0;
    std::snprintf(out, outSize, "%s", fen.c_str());
    return 1;
}
int cz_engine_load_network(CZEngine* engine, const char* path) {
    return engine && engine->impl.loadNetwork(path) ? 1 : 0;
}
int cz_engine_nnue_loaded(const CZEngine* engine) {
    return engine && engine->impl.nnueLoaded() ? 1 : 0;
}
int cz_engine_set_threads(CZEngine* engine, int threads) {
    return engine && engine->impl.setThreads(threads) ? 1 : 0;
}
int cz_engine_set_hash_mb(CZEngine* engine, std::size_t mb) {
    return engine && engine->impl.setHashMB(mb) ? 1 : 0;
}
int cz_engine_clear_hash(CZEngine* engine) {
    return engine && engine->impl.clearHash() ? 1 : 0;
}
int cz_engine_search(CZEngine* engine, int depth, int movetimeMs,
                     char* out, std::size_t outSize) {
    if (!engine || !out || outSize == 0) return 0;
    const cz::SearchResult r = engine->impl.search(depth, movetimeMs);
    if (!r.ok) {
        std::snprintf(out, outSize, "error|%s", engine->impl.lastError());
        return 0;
    }
    std::snprintf(out, outSize, "bestmove|%s|depth|%d|score|%d|nodes|%llu|pv|%s",
                  r.bestmove, r.depth, r.score,
                  static_cast<unsigned long long>(r.nodes), r.pv);
    return 1;
}
void cz_engine_stop(CZEngine* engine) {
    if (engine) engine->impl.stop();
}
void cz_engine_wait_idle(CZEngine* engine) {
    if (engine) engine->impl.waitIdle();
}
int cz_engine_is_searching(const CZEngine* engine) {
    return engine && engine->impl.isSearching() ? 1 : 0;
}
const char* cz_engine_last_error(const CZEngine* engine) {
    static const char* kNull = "invalid engine handle";
    return engine ? engine->impl.lastError() : kNull;
}
} // extern "C"
