#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace cz {

struct SearchResult {
    bool ok = false;
    int depth = 0;
    int score = 0;
    std::uint64_t nodes = 0;
    char bestmove[6]{};
    char pv[2048]{};
};

class Engine {
public:
    Engine();
    ~Engine();

    bool setPositionFEN(const char* fen);
    bool newGame();
    bool makeMoveUCI(const char* uci);
    bool undoMove();
    std::string currentFEN() const;
    bool loadNetwork(const char* path);
    bool nnueLoaded() const noexcept;
    bool setThreads(int threads);
    bool setHashMB(std::size_t mb);
    bool clearHash();

    SearchResult search(int depth, int movetimeMs);
    void stop();
    void waitIdle();
    bool isSearching() const noexcept;

    const char* lastError() const noexcept;

private:
    class Impl;
    Impl* impl_;
};

} // namespace cz

extern "C" {

struct CZEngine;

CZEngine* cz_engine_create();
void cz_engine_destroy(CZEngine* engine);

int cz_engine_set_position_fen(CZEngine* engine, const char* fen);
int cz_engine_new_game(CZEngine* engine);
int cz_engine_make_move_uci(CZEngine* engine, const char* uci);
int cz_engine_undo_move(CZEngine* engine);
int cz_engine_get_fen(const CZEngine* engine, char* out, std::size_t outSize);
int cz_engine_load_network(CZEngine* engine, const char* path);
int cz_engine_nnue_loaded(const CZEngine* engine);
int cz_engine_set_threads(CZEngine* engine, int threads);
int cz_engine_set_hash_mb(CZEngine* engine, std::size_t mb);
int cz_engine_clear_hash(CZEngine* engine);

int cz_engine_search(CZEngine* engine, int depth, int movetimeMs,
                     char* out, std::size_t outSize);
void cz_engine_stop(CZEngine* engine);
void cz_engine_wait_idle(CZEngine* engine);
int cz_engine_is_searching(const CZEngine* engine);
const char* cz_engine_last_error(const CZEngine* engine);

}
