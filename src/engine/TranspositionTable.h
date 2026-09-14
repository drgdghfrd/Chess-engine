#pragma once
#include "../chess/Move.h"
#include <cstdint>
#include <vector>

namespace uc {
enum class Bound : uint8_t { Exact, Lower, Upper };

struct TTEntry {
    uint64_t key=0;
    int depth=-1;
    int score=0;
    Bound bound=Bound::Exact;
    Move best{};
};

class TranspositionTable {
public:
    explicit TranspositionTable(size_t mb=32);
    void resize(size_t mb);
    void clear();
    bool probe(uint64_t key,int depth,int alpha,int beta,int& score,Move& best) const;
    void store(uint64_t key,int depth,int score,Bound bound,const Move& best);
private:
    std::vector<TTEntry> table_;
};
}
