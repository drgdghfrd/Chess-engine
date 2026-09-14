#include "../src/engine/Search.h"
#include "../src/chess/Position.h"
#include <chrono>
#include <iostream>
#include <vector>
using namespace uc;
struct Case{const char* name; const char* fen; int depth;};
int main(){
    const std::vector<Case> cases={
      {"start", "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", 6},
      {"kiwipete", "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", 4},
      {"tactical", "r4rk1/pp1b1ppp/2p1pn2/8/2B1P3/2N2N2/PPPQ1PPP/2R2RK1 w - - 0 1", 5}
    };
    uint64_t totalNodes=0;auto all=std::chrono::steady_clock::now();
    for(const auto& c:cases){Position p;p.setFEN(c.fen);Search s(64);auto t=std::chrono::steady_clock::now();Move m=s.go(p,c.depth);auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-t).count();uint64_t n=s.nodes();totalNodes+=n;double nps=ms?1000.0*n/ms:0;std::cout<<c.name<<" depth "<<c.depth<<" best "<<toUci(m)<<" score "<<s.score()<<" nodes "<<n<<" time_ms "<<ms<<" nps "<<(uint64_t)nps<<"\n";}
    auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-all).count();std::cout<<"total nodes "<<totalNodes<<" total_ms "<<ms<<"\n";
}
