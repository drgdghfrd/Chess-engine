#include "../src/chess/Position.h"
#include "../src/chess/Move.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
using namespace uc;
static Move parse(const Position& p, const std::string& u){
    if(u.size()<4) return {};
    int f=p.parseSq(u.substr(0,2)), t=p.parseSq(u.substr(2,2));
    if(f<0||t<0) return {};
    for(const auto&m:p.legal()) if(m.from==f&&m.to==t){
        if(!isPromotion(m) && u.size()==4) return m;
        if(isPromotion(m) && u.size()>=5){ char c=u[4]; int pp=promotionPiece(m);
            if((pp==2&&c=='n')||(pp==3&&c=='b')||(pp==4&&c=='r')||(pp==5&&c=='q')) return m; }
    }
    return {};
}
int main(int argc,char**argv){
    if(argc<3){std::cerr<<"usage: make_opening_book input.txt output.txt\n";return 2;}
    std::ifstream in(argv[1]); std::ofstream out(argv[2]); if(!in||!out)return 3;
    std::string line; size_t lines=0, entries=0, bad=0;
    while(std::getline(in,line)){
        if(line.empty()||line[0]=='#') continue;
        std::istringstream ss(line); std::string mv; std::vector<std::string> moves;
        while(ss>>mv) moves.push_back(mv);
        Position p; p.start();
        for(const auto& u:moves){
            Move m=parse(p,u); if(m.from==m.to && m.from==0){bad++;break;}
            out<<p.fen()<<"|"<<u<<"\n"; entries++;
            p.make(m);
        }
        lines++;
    }
    std::cerr<<"opening lines="<<lines<<" entries="<<entries<<" bad="<<bad<<"\n";
}
