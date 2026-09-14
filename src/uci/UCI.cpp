#include "UCI.h"
#include "../book/OpeningBook.h"
#include "../tablebase/Syzygy.h"
#include "../chess/Move.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <string>

namespace uc {

static Syzygy tb_;

UCI::UCI(){ book_.load("book.txt"); }

static Move parseMove(const Position&p,const std::string&t){
    if(t.size()<4)return{};
    int f=p.parseSq(t.substr(0,2)),to=p.parseSq(t.substr(2,2));
    if(f<0||to<0)return{};

    for(auto&m:p.legal()){
        if(m.from==f&&m.to==to){
            if(!isPromotion(m)&&t.size()==4)return m;
            if(isPromotion(m)&&t.size()>=5){
                char c=t[4];
                int pp=promotionPiece(m);
                if((pp==2&&c=='n')||(pp==3&&c=='b')||
                   (pp==4&&c=='r')||(pp==5&&c=='q')) return m;
            }
        }
    }
    return{};
}

void UCI::position(const std::string&line){
    std::istringstream ss(line);
    std::string x;
    ss>>x>>x;

    if(x=="startpos"){
        p_.start();
        ss>>x;
    }else if(x=="fen"){
        std::string f,z;
        for(int i=0;i<6&&ss>>z;i++){
            if(i)f+=' ';
            f+=z;
        }
        p_.setFEN(f);
        ss>>x;
    }

    if(x=="moves")
        while(ss>>x){
            auto m=parseMove(p_,x);
            p_.make(m);
        }
}

void UCI::setoption(const std::string&line){
    std::istringstream ss(line);
    std::string tok,name;
    ss>>tok>>tok;

    while(ss>>tok&&tok!="value"){
        if(!name.empty())name+=' ';
        name+=tok;
    }

    if(name=="Threads"){
        int n=1;
        if(ss>>n)s_.setThreads(std::clamp(n,1,64));
    }else if(name=="Hash"){
        int n=64;
        if(ss>>n)s_.setHashMB(static_cast<size_t>(std::clamp(n,1,2048)));
    }else if(name=="Clear Hash"){
        s_.clearHash();
    }else if(name=="EvalFile"){
        std::string f;
        if(ss>>f)network().load(f.c_str());
    }else if(name=="BookFile"){
        std::string f;
        if(ss>>f)book_.load(f);
    }else if(name=="UseBook"){
        std::string v;
        if(ss>>v)useBook_=(v=="true"||v=="1");
    }else if(name=="SyzygyPath"){
        std::string f;
        if(ss>>f)tb_.setPath(f);
    }else if(name=="SyzygyProbeLimit"){
        int n=5;
        if(ss>>n)tb_.setProbeLimit(n);
    }
}

void UCI::loop(){
    std::string l;

    while(std::getline(std::cin,l)){
        if(l=="uci"){
            std::cout<<"id name UltraChess v0.16\n"
                     <<"id author OpenAI-assisted\n"
                     <<"option name Threads type spin default 1 min 1 max 64\n"
                     <<"option name Hash type spin default 64 min 1 max 2048\n"
                     <<"option name EvalFile type string default\n"
                     <<"option name NNUEEmbedded type string default built-in\n"
                     <<"option name BookFile type string default book.txt\n"
                     <<"option name UseBook type check default true\n"
                     <<"option name SyzygyPath type string default\n"
                     <<"option name SyzygyProbeLimit type spin default 5 min 0 max 7\n"
                     <<"uciok\n";
        }else if(l=="isready"){
            std::cout<<"info string nnue="<<(network().loaded()?"embedded/file":"fallback")<<"\n";
            std::cout<<"readyok\n";
        }else if(l.rfind("setoption",0)==0){
            setoption(l);
        }else if(l.rfind("position",0)==0){
            position(l);
        }else if(l.rfind("perft",0)==0){
            std::istringstream ss(l);
            std::string a;int d;
            ss>>a>>d;
            std::cout<<"nodes "<<perft(p_,d)<<"\n";
        }else if(l=="stop"){
            s_.stop();
        }else if(l.rfind("go",0)==0){
            std::istringstream ss(l);
            std::string a,b;
            int d=16,ms=0,wtime=0,btime=0,winc=0,binc=0,mtg=0;
            ss>>a;

            while(ss>>a){
                if(a=="depth"&&ss>>b)d=std::clamp(std::stoi(b),1,64);
                else if(a=="movetime"&&ss>>b)ms=std::max(1,std::stoi(b));
                else if(a=="wtime"&&ss>>b)wtime=std::max(0,std::stoi(b));
                else if(a=="btime"&&ss>>b)btime=std::max(0,std::stoi(b));
                else if(a=="winc"&&ss>>b)winc=std::max(0,std::stoi(b));
                else if(a=="binc"&&ss>>b)binc=std::max(0,std::stoi(b));
                else if(a=="movestogo"&&ss>>b)mtg=std::max(0,std::stoi(b));
            }

            Move m{};

            if(useBook_){
                m=book_.probe(p_);
                if(m.from!=m.to||m.from!=0||m.flag!=Flag::Quiet){
                    std::cout<<"info string book\nbestmove "<<toUci(m)<<"\n";
                    continue;
                }
            }

            if(ms)m=s_.go(p_,d,ms);
            else if(wtime||btime)m=s_.goTimed(p_,d,wtime,btime,winc,binc,mtg);
            else m=s_.go(p_,d);

            std::cout<<"info depth "<<s_.completedDepth()
                     <<" score cp "<<s_.score()
                     <<" nodes "<<s_.nodes()<<"\n"
                     <<"bestmove "<<toUci(m)<<"\n";
        }else if(l=="book on"||l=="book off"||
                 l=="book status"||l=="book reload"||l=="book help"){
            if(l=="book on")useBook_=true;
            else if(l=="book off")useBook_=false;
            else if(l=="book reload")book_.load("book.txt");
            else if(l=="book help"){
                std::cout<<"info string commands: book on | book off | book status | book reload\n";
                continue;
            }

            std::cout<<"info string book enabled="<<(useBook_?"true":"false")
                     <<" loaded="<<(book_.loaded()?"true":"false")
                     <<" positions="<<book_.size()<<"\n";
        }else if(l=="d"){
            auto st=tb_.status(p_);
            auto legal=p_.legal();
            bool check=p_.inCheck(p_.side()==1);
            const char* result=
                legal.empty()?(check?"checkmate":"stalemate"):
                (check?"check":"play");

            std::cout<<p_.boardString()<<p_.fen()<<"\n"
                     <<"info string status="<<result
                     <<" legal="<<legal.size()
                     <<" tablebase pieces="<<st.pieces
                     <<" eligible="<<(st.eligible?"true":"false")
                     <<" configured="<<(st.configured?"true":"false")
                     <<" path_exists="<<(st.pathExists?"true":"false")
                     <<" probing="<<(st.probingAvailable?"true":"false")<<"\n";
        }else if(l=="quit"){
            break;
        }
    }
}

}
