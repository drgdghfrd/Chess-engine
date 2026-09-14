#include "TranspositionTable.h"
#include <algorithm>

namespace uc {

TranspositionTable::TranspositionTable(size_t mb){
    resize(mb);
}

void TranspositionTable::resize(size_t mb){
    // Keep memory predictable on Android; at least one entry.
    size_t n=std::max<size_t>(1,(mb*1024ULL*1024ULL)/sizeof(TTEntry));
    table_.clear();
    table_.resize(n);
}

void TranspositionTable::clear(){
    for(auto &e:table_) e=TTEntry{};
}

bool TranspositionTable::probe(uint64_t key,int depth,int alpha,int beta,int& score,Move& best) const{
    const auto&e=table_[key%table_.size()];
    if(e.key!=key)return false;
    best=e.best;
    if(e.depth<depth)return false;
    if(e.bound==Bound::Exact){score=e.score;return true;}
    if(e.bound==Bound::Lower&&e.score>=beta){score=e.score;return true;}
    if(e.bound==Bound::Upper&&e.score<=alpha){score=e.score;return true;}
    return false;
}

void TranspositionTable::store(uint64_t key,int depth,int score,Bound bound,const Move& best){
    auto&e=table_[key%table_.size()];
    // Prefer deeper information, while still replacing an old shallower entry.
    if(e.key!=key || depth>=e.depth || bound==Bound::Exact)
        e={key,depth,score,bound,best};
}

}
