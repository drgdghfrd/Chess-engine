#include "OpeningBook.h"
#include "PolyglotZobrist.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>

namespace uc {
std::string OpeningBook::key4(const std::string& fen){
    std::istringstream ss(fen); std::string a,b,c,d;
    if(!(ss>>a>>b>>c>>d)) return {};
    return a+" "+b+" "+c+" "+d;
}
uint16_t OpeningBook::readBE16(const unsigned char* p){ return uint16_t(p[0]<<8|p[1]); }
uint32_t OpeningBook::readBE32(const unsigned char* p){ return (uint32_t(p[0])<<24)|(uint32_t(p[1])<<16)|(uint32_t(p[2])<<8)|p[3]; }
uint64_t OpeningBook::readBE64(const unsigned char* p){
    uint64_t v=0; for(int i=0;i<8;++i) v=(v<<8)|p[i]; return v;
}

bool OpeningBook::load(const std::string& path){
    textEntries_.clear(); polyglotEntries_.clear(); loaded_=false; format_=BookFormat::None;
    std::error_code ec;
    if(!std::filesystem::is_regular_file(path,ec)) return false;
    const auto ext=std::filesystem::path(path).extension().string();
    if(ext==".bin" || ext==".BIN") {
        std::ifstream f(path,std::ios::binary); if(!f) return false;
        unsigned char rec[16];
        while(f.read(reinterpret_cast<char*>(rec),16)) {
            BookEntry e;
            e.key=readBE64(rec); e.rawMove=readBE16(rec+8); e.weight=readBE16(rec+10); e.learn=readBE32(rec+12);
            polyglotEntries_.push_back(e);
        }
        if(!f.eof()) { polyglotEntries_.clear(); return false; }
        std::sort(polyglotEntries_.begin(),polyglotEntries_.end(),[](const BookEntry&a,const BookEntry&b){return a.key<b.key;});
        loaded_=!polyglotEntries_.empty(); format_=loaded_?BookFormat::Polyglot:BookFormat::None; return loaded_;
    }
    std::ifstream f(path); if(!f) return false;
    std::string line;
    while(std::getline(f,line)){
        auto hash=line.find('#'); if(hash!=std::string::npos) line.resize(hash);
        if(line.empty()) continue;
        std::istringstream ss(line); std::string key,move;
        std::getline(ss,key,'|'); ss>>move;
        if(key.empty()||move.empty()) continue;
        const auto k=key4(key); if(k.empty()) continue;
        textEntries_[k].push_back(move);
    }
    loaded_=!textEntries_.empty(); format_=loaded_?BookFormat::Text:BookFormat::None; return loaded_;
}

const char* OpeningBook::formatName() const {
    switch(format_){case BookFormat::Text:return "text"; case BookFormat::Polyglot:return "polyglot"; default:return "none";}
}

Move OpeningBook::probeText(const Position& p) const{
    auto it=textEntries_.find(key4(p.fen())); if(it==textEntries_.end()) return {};
    std::vector<Move> candidates;
    for(const auto& u:it->second){
        if(u.size()<4) continue;
        int from=p.parseSq(u.substr(0,2)), to=p.parseSq(u.substr(2,2)); if(from<0||to<0) continue;
        for(const auto&m:p.legal()) if(m.from==from&&m.to==to){
            if(isPromotion(m)){ if(u.size()<5) continue; char c=u[4]; int pp=promotionPiece(m); if((pp==2&&c=='n')||(pp==3&&c=='b')||(pp==4&&c=='r')||(pp==5&&c=='q')) candidates.push_back(m); }
            else if(u.size()==4) candidates.push_back(m);
        }
    }
    if(candidates.empty()) return {};
    return candidates[std::hash<std::string>{}(p.fen())%candidates.size()];
}

Move OpeningBook::decodePolyglotMove(const Position& p,uint16_t raw){
    const int to=raw&63, from=(raw>>6)&63, promo=(raw>>12)&7;
    for(const auto&m:p.legal()) if(m.from==from&&m.to==to){
        if(!isPromotion(m) && promo==0) return m;
        if(isPromotion(m)){
            int pp=promotionPiece(m);
            if((promo==1&&pp==2)||(promo==2&&pp==3)||(promo==3&&pp==4)||(promo==4&&pp==5)) return m;
        }
    }
    return {};
}

Move OpeningBook::probePolyglotKey(const Position& p,uint64_t key) const{
    if(!loaded_||format_!=BookFormat::Polyglot) return {};
    auto lo=std::lower_bound(polyglotEntries_.begin(),polyglotEntries_.end(),key,[](const BookEntry&e,uint64_t k){return e.key<k;});
    if(lo==polyglotEntries_.end()||lo->key!=key) return {};
    auto hi=lo; while(hi!=polyglotEntries_.end()&&hi->key==key) ++hi;
    uint32_t total=0; for(auto it=lo;it!=hi;++it) total+=it->weight;
    if(total==0) return decodePolyglotMove(p,lo->rawMove);
    uint32_t pick=uint32_t(std::hash<uint64_t>{}(key)%total);
    for(auto it=lo;it!=hi;++it){ if(pick<it->weight) return decodePolyglotMove(p,it->rawMove); pick-=it->weight; }
    return {};
}

Move OpeningBook::probe(const Position& p) const{
    if(!loaded_) return {};
    // Text books use FEN keys; binary books use the canonical PolyGlot hash.
    if (format_==BookFormat::Text) return probeText(p);
    return probePolyglot(p);
}
Move OpeningBook::probePolyglot(const Position& p) const {
    if (!PolyglotZobrist::isCanonicalTableReady()) return {};
    return probePolyglotKey(p, PolyglotZobrist::key(p));
}

} // namespace uc
