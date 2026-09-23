#include "UCI.h"
#include "../Version.h"
#include "../engine/Evaluation.h"
#include "../book/OpeningBook.h"
#include "../book/PolyglotZobrist.h"
#include "../tablebase/Syzygy.h"
#include "../chess/Move.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <string>
#include <utility>

namespace uc {

static Syzygy tb_;

UCI::UCI(){
    // UCI is a streaming protocol: replies must be visible immediately even
    // when stdout is a pipe connected to a GUI/test harness.
    std::cout.setf(std::ios::unitbuf);
    s_.setSyzygy(&tb_);
    book_.load(bookFile_);

    // Android/OEX builds carry the HalfKP network inside the executable, so
    // Chess GUIs do not need filesystem access to the app's private assets.
    // Desktop builds retain the historical file-based fallback.
    if (!network().loadEmbedded()) {
        const char* candidates[] = {
            "nets/ChessZero-v0.75-halfkp.nnue",
            "../nets/ChessZero-v0.75-halfkp.nnue",
            "ChessZero-v0.75-halfkp.nnue",
            "nets/ChessZero-v0.32-demo.nnue",
            "../nets/ChessZero-v0.32-demo.nnue",
            "ChessZero-v0.32-demo.nnue"
        };
        for (const char* f : candidates) if (network().load(f)) break;
    }
}

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
            if (m.from == m.to && m.from == 0 && m.flag == Flag::Quiet) {
                std::cout << "info string position invalid_move=" << x << " ignored\n";
                continue;
            }
            if (!p_.make(m)) {
                std::cout << "info string position illegal_move=" << x << " ignored\n";
            }
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
        int n=32;
        if(ss>>n)s_.setHashMB(static_cast<size_t>(std::clamp(n,1,2048)));
    }else if(name=="Move Overhead"){
        int n=30;
        if(ss>>n){ moveOverheadMs_=std::clamp(n,0,1000); s_.setMoveOverheadMs(moveOverheadMs_); }
    }else if(name=="Slow Mover"){
        int n=100;
        if(ss>>n){ slowMover_=std::clamp(n,10,1000); s_.setSlowMover(slowMover_); }
    }else if(name=="Clear Hash"){
        s_.clearHash();
    }else if(name=="Aspiration"){
        std::string v;
        if(ss>>v) s_.setAspiration(v=="true"||v=="1");
    }else if(name=="AspirationWindow"){
        int n=20;
        if(ss>>n) s_.setAspirationWindow(n);
    }else if(name=="LMR"){
        std::string v;
        if(ss>>v) s_.setLMR(v=="true"||v=="1");
    }else if(name=="LMRAggression"){
        int n=100;
        if(ss>>n) s_.setLMRAggression(n);
    }else if(name=="NullMove"){
        std::string v;
        if(ss>>v) s_.setNullMove(v=="true"||v=="1");
    }else if(name=="NullMoveAggression"){
        int n=100;
        if(ss>>n) s_.setNullMoveAggression(n);
    }else if(name=="NullMoveVerification"){
        std::string v;
        if(ss>>v) s_.setNullMoveVerification(v=="true"||v=="1");
    }else if(name=="History"){
        std::string v;
        if(ss>>v) s_.setHistory(v=="true"||v=="1");
    }else if(name=="HistoryAggression"){
        int n=100; if(ss>>n) s_.setHistoryAggression(n);
    }else if(name=="Countermove"){
        std::string v;
        if(ss>>v) s_.setCountermove(v=="true"||v=="1");
    }else if(name=="CountermoveAggression"){
        int n=100; if(ss>>n) s_.setCountermoveAggression(n);
    }else if(name=="HistoryPruning"){
        std::string v; if(ss>>v) s_.setHistoryPruning(v=="true"||v=="1");
    }else if(name=="HistoryPruningAggression"){
        int n=100; if(ss>>n) s_.setHistoryPruningAggression(n);
    }else if(name=="FutilityAggression"){
        int n=100; if(ss>>n) s_.setFutilityAggression(n);
    }else if(name=="Razoring"){
        std::string v;
        if(ss>>v) s_.setRazoring(v=="true"||v=="1");
    }else if(name=="RazoringAggression"){
        int n=100;
        if(ss>>n) s_.setRazoringAggression(n);
    }else if(name=="EvalMode"){
        std::string v;
        if(ss>>v){
            if(v=="classical"||v=="Classical") setEvalMode(EvalMode::Classical);
            else setEvalMode(EvalMode::NNUE);
            std::cout<<"info string evalmode="<<(evalMode()==EvalMode::NNUE?"nnue":"classical")<<"\n";
        }
    }else if(name=="EvalFile"){
        std::string f;
        if(ss>>f){
            const bool wantsEmbedded = (f=="built-in" || f=="builtin" || f=="embedded" || f=="embedded://ChessZero-v0.75-halfkp.nnue");
            const bool ok = wantsEmbedded ? network().loadEmbedded() : network().load(f.c_str());
            if(!ok && !network().loaded())
                std::cout<<"info string nnue=load_failed arch=HalfKP-40960x256 file="<<f<<" fallback=classic\n";
            else if(!ok)
                std::cout<<"info string nnue=loaded arch=HalfKP-40960x256 source="<<network().file()<<" requested="<<f<<"\n";
            else
                std::cout<<"info string nnue=loaded arch=HalfKP-40960x256 file="<<network().file()<<"\n";
        }
    }else if(name=="NNUEEmbedded"){
        std::string v;
        if(ss>>v){
            nnueEmbedded_ = (v=="true" || v=="1");
            if(nnueEmbedded_){
                const bool ok = network().loadEmbedded();
                setEvalMode(EvalMode::NNUE);
                std::cout<<"info string nnue_embedded="<<(ok?"enabled":"unavailable")<<" evalmode=nnue source="<<network().file()<<"\n";
            }else{
                setEvalMode(EvalMode::Classical);
                std::cout<<"info string nnue_embedded=disabled evalmode=classical\n";
            }
        }
    }else if(name=="BookFile"){
        std::string f;
        std::getline(ss, f);
        while(!f.empty() && (f.front()==' ' || f.front()=='\t')) f.erase(f.begin());
        bookFile_=f;
        if(bookFile_.empty()){
            book_.load("__disabled_book__");
            std::cout<<"info string book file=<unset> loaded=false\n";
        }else if(book_.load(bookFile_)){
            std::cout<<"info string book file="<<bookFile_<<" loaded=true format="<<book_.formatName()<<" entries="<<book_.size()<<"\n";
        }else{
            std::cout<<"info string book file="<<bookFile_<<" loaded=false format=none entries=0\n";
        }
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

UCI::~UCI(){
    waitSearch(true);
}

void UCI::waitSearch(bool requestStop){
    if (requestStop) {
        cancelRequested_.store(true, std::memory_order_release);
        s_.stop();
    }
    if (searchThread_.joinable())
        searchThread_.join();
}

void UCI::emitSearchResult(const Move& m){
    std::string pv = s_.pvString();
    std::cout<<"info depth "<<s_.completedDepth()
             <<" score cp "<<s_.score()
             <<" timebudget_soft "<<s_.budgetSoftMs()
             <<" timebudget_hard "<<s_.budgetHardMs()
             <<" nodes "<<s_.nodes()
             <<" aspiration "<<s_.aspirationSearches()
             <<" asp_fail_low "<<s_.aspirationFailLows()
             <<" asp_fail_high "<<s_.aspirationFailHighs()
             <<" pvs_research "<<s_.pvsResearches();
    if(!pv.empty()) std::cout<<" pv "<<pv;
    std::cout<<"\n";
    if (p_.isDraw())
        std::cout<<"info string draw="<<p_.drawReason()<<"\n";
    std::cout<<"bestmove "<<toUci(m)<<"\n"<<std::flush;
}

void UCI::startSearch(const std::string& line){
    waitSearch(true);

    std::istringstream ss(line);
    std::string a,b;
    int d=16,ms=0,wtime=0,btime=0,winc=0,binc=0,mtg=0;
    bool clockSpecified=false;
    bool infinite=false;
    ss>>a;

    auto readInt = [&](const std::string& tok, int& dst, int lo, int hi)->bool {
        try {
            dst = std::clamp(std::stoi(tok), lo, hi);
            return true;
        } catch (...) { return false; }
    };

    while(ss>>a){
        if(a=="depth"&&ss>>b) readInt(b,d,1,64);
        else if(a=="movetime"&&ss>>b) readInt(b,ms,1,24*60*60*1000);
        else if(a=="wtime"&&ss>>b){ clockSpecified=true; readInt(b,wtime,0,24*60*60*1000); }
        else if(a=="btime"&&ss>>b){ clockSpecified=true; readInt(b,btime,0,24*60*60*1000); }
        else if(a=="winc"&&ss>>b){ clockSpecified=true; readInt(b,winc,0,24*60*60*1000); }
        else if(a=="binc"&&ss>>b){ clockSpecified=true; readInt(b,binc,0,24*60*60*1000); }
        else if(a=="movestogo"&&ss>>b) readInt(b,mtg,0,200);
        else if(a=="infinite") infinite=true;
        else if(a=="ponder") { /* v0.97: accepted and treated as a normal search. */ }
    }

    if(useBook_){
        Move bm=book_.probe(p_);
        if(bm.from!=bm.to||bm.from!=0||bm.flag!=Flag::Quiet){
            std::cout<<"info string book\nbestmove "<<toUci(bm)<<"\n"<<std::flush;
            return;
        }
    }

    // Snapshot the position before launching the worker. The search mutates
    // and restores its Position internally; keeping a private copy also means
    // a GUI can send stop/isready/quit without racing the main position.
    Position searchPos = p_;
    cancelRequested_.store(false, std::memory_order_release);
    searching_.store(true, std::memory_order_release);
    searchThread_ = std::thread([this, searchPos, d, ms, wtime, btime, winc, binc, mtg, clockSpecified, infinite]() mutable {
        Move m{};
        if (cancelRequested_.load(std::memory_order_acquire)) {
            auto legal = searchPos.legal();
            if (!legal.empty()) m = legal.front();
            searching_.store(false, std::memory_order_release);
            emitSearchResult(m);
            return;
        }
        if(ms) m=s_.go(searchPos,d,ms);
        else if(clockSpecified) m=s_.goTimed(searchPos,d,wtime,btime,winc,binc,mtg);
        else m=s_.go(searchPos,infinite ? 64 : d);
        searching_.store(false, std::memory_order_release);
        emitSearchResult(m);
    });
}

void UCI::loop(){
    std::string l;

    while(std::getline(std::cin,l)){
        if(l=="uci"){
            waitSearch(true);
            std::cout<<"id name ChessZero v" << kVersion << "\n"
                     <<"id author " << kAuthor << "\n"
                     <<"option name Threads type spin default 1 min 1 max 64\n"
                     <<"option name Hash type spin default 32 min 1 max 2048\n"
                     <<"option name Move Overhead type spin default 30 min 0 max 1000\n"
                     <<"option name Slow Mover type spin default 100 min 10 max 1000\n"
                     <<"option name Aspiration type check default true\n"
                     <<"option name AspirationWindow type spin default 20 min 4 max 128\n"
                     <<"option name LMR type check default true\n"
                     <<"option name LMRAggression type spin default 100 min 50 max 200\n"
                     <<"option name NullMove type check default true\n"
                     <<"option name NullMoveAggression type spin default 100 min 50 max 200\n"
                     <<"option name NullMoveVerification type check default true\n"
                     <<"option name History type check default true\n"
                     <<"option name HistoryAggression type spin default 100 min 50 max 200\n"
                     <<"option name Countermove type check default true\n"
                     <<"option name CountermoveAggression type spin default 100 min 50 max 200\n"
                     <<"option name HistoryPruning type check default true\n"
                     <<"option name HistoryPruningAggression type spin default 100 min 50 max 200\n"
                     <<"option name FutilityAggression type spin default 100 min 50 max 200\n"
                     <<"option name Razoring type check default true\n"
                     <<"option name RazoringAggression type spin default 100 min 50 max 200\n"
                     <<"option name EvalMode type combo default nnue var nnue var classical\n"
                     <<"option name EvalFile type string default embedded://ChessZero-v0.75-halfkp.nnue\n"
                     <<"option name NNUEEmbedded type check default true\n"
                     <<"option name BookFile type string default book.txt\n"
                     <<"option name UseBook type check default true\n"
                     <<"option name SyzygyPath type string default\n"
                     <<"option name SyzygyProbeLimit type spin default 5 min 0 max 7\n"
                     <<"uciok\n";
        }else if(l=="eval"){
            printClassicalEval(p_, std::cout);
            // Also show NNUE/full evaluate if available
            std::cout << "info string eval full(evaluate) stm_cp="
                      << evaluate(p_) << std::endl;
        }else if(l=="isready"){
            waitSearch(true);
            std::cout<<"info string evalmode="<<(evalMode()==EvalMode::NNUE?"nnue":"classical")<<" nnue="<<(network().loaded()?"loaded":"fallback")<<" arch=HalfKP-40960x256 simd="<<nnueSimdPath()<<"\n";
            std::cout<<"readyok\n";
        }else if(l.rfind("setoption",0)==0){
            waitSearch(true);
            setoption(l);
        }else if(l.rfind("position",0)==0){
            waitSearch(true);
            position(l);
        }else if(l=="ucinewgame"){
            waitSearch(true);
            s_.clearHash();
            std::cout<<"info string newgame=cleared\n";
        }else if(l.rfind("perft",0)==0){
            std::istringstream ss(l);
            std::string a;int d;
            ss>>a>>d;
            std::cout<<"nodes "<<perft(p_,d)<<"\n";
        }else if(l=="stop"){
            waitSearch(true);
        }else if(l.rfind("go",0)==0){
            startSearch(l);
        }else if(l=="book on"||l=="book off"||
                 l=="book status"||l=="book reload"||l=="book help"){
            if(l=="book on")useBook_=true;
            else if(l=="book off")useBook_=false;
            else if(l=="book reload"){
                bool ok=!bookFile_.empty() && book_.load(bookFile_);
                std::cout<<"info string book reload="<<(ok?"ok":"failed")<<" file="<<(bookFile_.empty()?"<unset>":bookFile_)<<"\n";
            }else if(l=="book help"){
                std::cout<<"info string commands: book on | book off | book status | book reload | book key | book probe\n";
                continue;
            }

            std::cout<<"info string book enabled="<<(useBook_?"true":"false")
                     <<" loaded="<<(book_.loaded()?"true":"false")
                     <<" format="<<book_.formatName()
                     <<" entries="<<book_.size()
                     <<" file="<<(bookFile_.empty()?"<unset>":bookFile_)
                     <<" polyglot_hash="<<(PolyglotZobrist::isCanonicalTableReady()?"ready":"unavailable")<<"\n";
        }else if(l=="book key"){
            if(PolyglotZobrist::isCanonicalTableReady())
                std::cout<<"info string polyglot key="<<std::hex<<PolyglotZobrist::key(p_)<<std::dec<<"\n";
            else std::cout<<"info string polyglot key=unavailable\n";
        }else if(l=="book probe"){
            Move bm=book_.probe(p_);
            if(bm.from!=bm.to||bm.from!=0||bm.flag!=Flag::Quiet)
                std::cout<<"info string book probe="<<toUci(bm)<<"\n";
            else std::cout<<"info string book probe=none\n";
        }else if(l=="d"){
            auto st=tb_.status(p_);
            auto legal=p_.legal();
            bool check=p_.inCheck(p_.side()==1);
            const char* result=
                legal.empty()?(check?"checkmate":"stalemate"):
                (p_.isDraw()?"draw":(check?"check":"play"));

            std::cout<<p_.boardString()<<p_.fen()<<"\n"
                     <<"info string status="<<result
                     <<" draw="<<(p_.isDraw()?p_.drawReason():"none")
                     <<" repetitions="<<p_.repetitionCount()
                     <<" halfmove="<<p_.halfmoveClock()
                     <<" legal="<<legal.size()
                     <<" tablebase pieces="<<st.pieces
                     <<" eligible="<<(st.eligible?"true":"false")
                     <<" configured="<<(st.configured?"true":"false")
                     <<" path_exists="<<(st.pathExists?"true":"false")
                     <<" probing="<<(st.probingAvailable?"true":"false")
                     <<" wdl_files="<<(st.wdlFiles?"true":"false")
                     <<" dtz_files="<<(st.dtzFiles?"true":"false")<<"\n";
        }else if(l=="tb status"){
            auto st=tb_.status(p_);
            std::cout<<"info string syzygy path="<<(tb_.path().empty()?"<unset>":tb_.path())
                     <<" probe_limit="<<st.probeLimit
                     <<" pieces="<<st.pieces
                     <<" largest="<<st.largest
                     <<" eligible="<<(st.eligible?"true":"false")
                     <<" configured="<<(st.configured?"true":"false")
                     <<" path_exists="<<(st.pathExists?"true":"false")
                     <<" probing="<<(st.probingAvailable?"true":"false")
                     <<" wdl_files="<<(st.wdlFiles?"true":"false")
                     <<" dtz_files="<<(st.dtzFiles?"true":"false")<<"\n";
        }else if(l=="tb probe") {
            auto st=tb_.status(p_); SyzygyProbeResult r; Move rm{};
            bool ok=tb_.rootProbe(p_,rm,r);
            if(!ok) r=tb_.probe(p_);
            std::cout<<"info string syzygy_probe eligible="<<(st.eligible?"true":"false")
                     <<" probing="<<(st.probingAvailable?"true":"false")
                     <<" largest="<<st.largest
                     <<" wdl_available="<<(r.wdlAvailable?"true":"false")
                     <<" dtz_available="<<(r.dtzAvailable?"true":"false")
                     <<" dtz_plies="<<r.dtzPlies
                     <<" root_move="<<(ok?toUci(rm):"0000")<<"\n";
        }else if(l=="quit"){
            waitSearch(true);
            break;
        }
    }
}

}
