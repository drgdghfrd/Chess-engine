#include "../src/chess/Position.h"
#include <iostream>
#include <vector>
#include <string>
using namespace uc;
struct Case{std::string fen;int depth;uint64_t expected;};
int main(){
    std::vector<Case> cases={
      {"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",4,197281ULL},
      {"8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1",4,43238ULL},
      {"r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1",3,97862ULL}
    };
    for(size_t i=0;i<cases.size();++i){Position p;if(!p.setFEN(cases[i].fen)){std::cerr<<"bad fen "<<i<<"\n";return 2;}auto got=perft(p,cases[i].depth);std::cout<<"case "<<i+1<<": "<<got<<" / "<<cases[i].expected<<"\n";if(got!=cases[i].expected)return 1;}
    std::cout<<"all perft regressions passed\n";return 0;
}
