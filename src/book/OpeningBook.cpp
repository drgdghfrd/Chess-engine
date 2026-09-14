#include "OpeningBook.h"
#include <fstream>
#include <sstream>
#include <algorithm>

namespace uc {
std::string OpeningBook::key4(const std::string& fen){
    std::istringstream ss(fen); std::string a,b,c,d;
    if(!(ss>>a>>b>>c>>d)) return {};
    return a+" "+b+" "+c+" "+d;
}

bool OpeningBook::load(const std::string& path){
    entries_.clear(); loaded_=false;
    std::ifstream f(path); if(!f) return false;
    std::string line;
    while(std::getline(f,line)){
        auto hash=line.find('#'); if(hash!=std::string::npos) line.resize(hash);
        if(line.empty()) continue;
        std::istringstream ss(line); std::string key,move;
        std::getline(ss,key,'|'); ss>>move;
        if(key.empty()||move.empty()) continue;
        const auto k = key4(key);
        if(k.empty()) continue;
        entries_[k].push_back(move);
    }
    loaded_=!entries_.empty(); return loaded_;
}

Move OpeningBook::probe(const Position& p) const{
    if(!loaded_) return {};
    auto it=entries_.find(key4(p.fen()));
    if(it==entries_.end()) return {};
    std::vector<Move> candidates;
    for(const auto& u:it->second){
        if(u.size()<4) continue;
        int from=p.parseSq(u.substr(0,2)), to=p.parseSq(u.substr(2,2));
        if(from<0||to<0) continue;
        for(const auto&m:p.legal()){
            if(m.from!=from||m.to!=to) continue;
            if(isPromotion(m)){
                if(u.size()<5) continue;
                const char c=u[4]; int pp=promotionPiece(m);
                if((pp==2&&c=='n')||(pp==3&&c=='b')||(pp==4&&c=='r')||(pp==5&&c=='q')) candidates.push_back(m);
            } else if(u.size()==4) candidates.push_back(m);
        }
    }
    if(candidates.empty()) return {};
    // Deterministic variety: the same position gets the same book move,
    // while positions with several book continuations are not always forced
    // to the first line loaded from the file.
    std::hash<std::string> h;
    return candidates[h(p.fen()) % candidates.size()];
}}
