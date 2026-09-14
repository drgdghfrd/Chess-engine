#pragma once
#include "TranspositionTable.h"
#include "Evaluation.h"
#include "../chess/Position.h"
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <thread>
#include <vector>

namespace uc {
class Search {
public:
    explicit Search(size_t ttMB=64);
    Move go(Position&,int depth);
    Move go(Position&,int depth,int movetimeMs);
    Move goTimed(Position&, int maxDepth, int wtimeMs, int btimeMs, int wincMs, int bincMs, int movesToGo=0);
    void stop();
    void setThreads(int n){threads_=n<1?1:n;}
    int threads()const{return threads_;}
    uint64_t nodes()const{return nodes_.load();}
    int score()const{return score_;}
    int completedDepth()const{return completedDepth_;}
    void setHashMB(size_t mb);
    void clearHash();

private:
    static constexpr int MAX_PLY=128;
    static constexpr int MAX_HISTORY=32768;
    TranspositionTable tt_;
    std::atomic<bool> stop_{false};
    std::atomic<bool>* externalStop_=nullptr;
    std::chrono::steady_clock::time_point deadline_{};
    bool timed_=false;
    std::atomic<uint64_t> nodes_{0};
    int score_=0,completedDepth_=0,threads_=1;
    Move rootBest_{};
    std::array<Move,MAX_PLY> killers1_{};
    std::array<Move,MAX_PLY> killers2_{};
    int history_[2][64][64]{};
    Move pvMove_{};

    bool timeUp();
    uint64_t hash(const Position&)const;
    int pieceValue(int p)const;
    int moveScore(const Position&,const Move&,int ply,const Move& ttMove)const;
    int qsearch(Position&,NNUE::Accumulator&,int,int,int);
    int negamax(Position&,NNUE::Accumulator&,int,int,int,int,bool allowNull=true);
    int seeCapture(const Position&,const Move&) const;
    int rootSearch(Position&,NNUE::Accumulator&,int,int,int,Move&);
    int rootSearchParallel(Position&,NNUE::Accumulator&,int,int,int,Move&);
    void updateHistory(int side,const Move&,int depth,int bonusSign=1);
    Move searchInternal(Position&,int,int);
};
}
