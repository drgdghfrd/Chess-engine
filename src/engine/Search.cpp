#include "Search.h"
#include "Evaluation.h"
#include "../chess/Move.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <thread>

namespace uc {

static constexpr int INF=30000;
static constexpr int MATE=29000;
static constexpr int TB_WIN=28000;

static uint64_t mix64(uint64_t x){
    x+=0x9e3779b97f4a7c15ULL;
    x=(x^(x>>30))*0xbf58476d1ce4e5b9ULL;
    x=(x^(x>>27))*0x94d049bb133111ebULL;
    return x^(x>>31);
}

Search::Search(size_t ttMB):tt_(ttMB){}

void Search::setHashMB(size_t mb){ tt_.resize(std::max<size_t>(1,mb)); }
void Search::clearHash(){ tt_.clear(); }

void Search::stop(){stop_=true;}

bool Search::timeUp(){
    if(stop_) return true;
    if(externalStop_ && externalStop_->load(std::memory_order_relaxed)) return true;
    if(timed_ && ((nodes_.load(std::memory_order_relaxed)&2047ULL)==0) &&
       std::chrono::steady_clock::now()>=deadline_){
        stop_=true;
        return true;
    }
    return false;
}

uint64_t Search::hash(const Position&p)const{
    auto bb=p.bitboards();
    uint64_t h=0x9e3779b97f4a7c15ULL;
    for(int i=0;i<12;i++){
        uint64_t x=bb.piece[i]+0x9e3779b97f4a7c15ULL*(i+1);
        h=mix64(h^x);
    }
    h=mix64(h ^ (p.side()==1?0xabcddcba12345678ULL:0x13579bdf2468ace0ULL));
    h=mix64(h ^ (0x1000ULL + static_cast<uint64_t>(p.castlingRights())));
    h=mix64(h ^ (0x2000ULL + static_cast<uint64_t>(p.epSquare()+1)));
    return mix64(h);
}

int Search::pieceValue(int p)const{
    static const int v[7]={0,100,320,330,500,900,20000};
    return v[std::abs(p)];
}

int Search::moveScore(const Position&p,const Move&m,int ply,const Move&ttMove)const{
    if(m==ttMove) return 3000000;

    int s=0;
    int attacker=pieceValue(p.at(m.from));
    int victim=0;
    if(isCapture(m)){
        if(m.flag==Flag::EnPassant) victim=100;
        else victim=pieceValue(p.at(m.to));
        // MVV-LVA style ordering: victim value dominates.
        s += 1000000 + 16*victim - attacker;
    }
    if(isPromotion(m))
        s += 800000 + pieceValue(promotionPiece(m))*16;

    if(ply<MAX_PLY){
        if(m==killers1_[ply]) s+=650000;
        else if(m==killers2_[ply]) s+=600000;
    }

    int side=p.side()==1?0:1;
    s+=history_[side][m.from][m.to];
    return s;
}

void Search::updateHistory(int side,const Move&m,int depth,int bonusSign){
    int &h=history_[side][m.from][m.to];
    int delta=std::clamp(depth*depth*bonusSign*12,-MAX_HISTORY,MAX_HISTORY);
    h += delta;
    h -= h*std::abs(delta)/(MAX_HISTORY+1);
    h=std::clamp(h,-MAX_HISTORY,MAX_HISTORY);
}

int Search::qsearch(Position&p,NNUE::Accumulator& acc,int a,int b,int ply){
    if(timeUp()) return 0;
    ++nodes_;

    const bool check=p.inCheck(p.side()==1);
    int stand=evaluate(p,acc);

    if(!check){
        if(stand>=b) return b;
        if(stand>a) a=stand;
    }

    auto mv=p.legal();
    std::stable_sort(mv.begin(),mv.end(),[&](const Move&x,const Move&y){
        return moveScore(p,x,ply,Move{})>moveScore(p,y,ply,Move{});
    });

    for(const auto&m:mv){
        // In check, every legal evasion must be searched.
        if(!check && !isCapture(m) && !isPromotion(m)) continue;

        // Cheap delta pruning for captures in non-check qsearch.
        if(!check && isCapture(m) && !isPromotion(m)){
            int gain=(m.flag==Flag::EnPassant)?100:pieceValue(p.at(m.to));
            if(stand + gain + 90 < a) continue;
        }

        int moved=p.at(m.from);
        int captured=isCapture(m)?p.at(m.to):0;
        if(m.flag==Flag::EnPassant)
            captured=p.at(m.to+(moved>0?-8:8));

        if(!p.make(m)) continue;

        NNUE::Accumulator child=acc;
        network().update(p,m,moved,captured,m.flag==Flag::EnPassant,child);

        int s=-qsearch(p,child,-b,-a,ply+1);
        p.undo();

        if(timeUp()) return 0;
        if(s>=b) return b;
        if(s>a) a=s;
    }
    return a;
}

// v0.14 SEE: an exchange sequence on the target square.
// It is deliberately used only for pruning/capture ordering, not as evaluation.
int Search::seeCapture(const Position&p,const Move&m) const{
    if(!isCapture(m)) return 0;

    int firstCaptured=(m.flag==Flag::EnPassant)
        ? p.at(m.to+(p.at(m.from)>0?-8:8))
        : p.at(m.to);

    int firstGain=pieceValue(firstCaptured);
    if(isPromotion(m))
        firstGain += pieceValue(promotionPiece(m))-pieceValue(p.at(m.from));

    Position cur=p;
    if(!cur.make(m)) return -INF;

    const int target=m.to;
    int gain[32];
    gain[0]=firstGain;
    int ply=0;

    while(ply<30){
        auto moves=cur.legal();
        Move best{};
        int bestVal=INF;
        bool found=false;

        for(const auto &x:moves){
            if(!isCapture(x) || x.to!=target) continue;
            int av=pieceValue(cur.at(x.from));
            if(isPromotion(x))
                av += pieceValue(promotionPiece(x))-pieceValue(cur.at(x.from));
            if(av<bestVal){
                bestVal=av;
                best=x;
                found=true;
            }
        }
        if(!found) break;

        ++ply;
        int victim=pieceValue(cur.at(target));
        int attacker=pieceValue(cur.at(best.from));
        int promoBonus=0;
        if(isPromotion(best))
            promoBonus=pieceValue(promotionPiece(best))-attacker;

        gain[ply]=victim+promoBonus-attacker;

        if(!cur.make(best)) break;
    }

    while(ply>0){
        --ply;
        gain[ply]=std::max(-gain[ply+1],gain[ply]);
    }
    return gain[0];
}

int Search::negamax(Position&p,NNUE::Accumulator& acc,int d,int a,int b,int ply,bool allowNull){
    if(timeUp()) return 0;
    ++nodes_;

    const bool check=p.inCheck(p.side()==1);
    if(d<=0) return qsearch(p,acc,a,b,ply);

    uint64_t key=hash(p);
    int ttScore=0;
    Move ttMove{};
    if(tt_.probe(key,d,a,b,ttScore,ttMove)) return ttScore;

    int staticEval=evaluate(p,acc);

    // Mate distance boundaries.
    a=std::max(a,-MATE+ply);
    b=std::min(b,MATE-ply);
    if(a>=b) return a;

    // Reverse futility / static null pruning.
    if(d<=3 && !check && std::abs(staticEval)<TB_WIN){
        const int margin=110*d;
        if(staticEval-margin>=b)
            return staticEval;
        if(d==1 && staticEval+120<=a)
            return qsearch(p,acc,a,b,ply);
    }

    // Razoring: only near the horizon and only when clearly below alpha.
    if(d<=2 && !check && staticEval+220*d<a)
        return qsearch(p,acc,a,b,ply);

    // Null move with a verification search at high depths.
    if(d>=3 && !check && allowNull){
        auto bb=p.bitboards();
        uint64_t nonPawns=
            (bb.piece[1]|bb.piece[2]|bb.piece[3]|bb.piece[4]|bb.piece[5]|
             bb.piece[7]|bb.piece[8]|bb.piece[9]|bb.piece[10]|bb.piece[11]);

        if(nonPawns && staticEval>=b-80){
            int r=(d>=8?3:2);
            p.makeNull();

            NNUE::Accumulator child;
            network().refresh(p,child);

            int ns=-negamax(p,child,d-1-r,-b,-b+1,ply+1,false);
            p.undoNull();

            if(timeUp()) return 0;

            if(ns>=b){
                if(d>=9){
                    NNUE::Accumulator verifyAcc=acc;
                    int verify=-negamax(p,verifyAcc,d-r-1,-b,-b+1,ply+1,false);
                    if(timeUp()) return 0;
                    if(verify>=b){
                        tt_.store(key,d,verify,Bound::Lower,ttMove);
                        return verify;
                    }
                } else {
                    tt_.store(key,d,ns,Bound::Lower,ttMove);
                    return ns;
                }
            }
        }
    }

    auto mv=p.legal();
    if(mv.empty()) return check ? -MATE+ply : 0;

    std::stable_sort(mv.begin(),mv.end(),[&](const Move&x,const Move&y){
        return moveScore(p,x,ply,ttMove)>moveScore(p,y,ply,ttMove);
    });

    int best=-INF;
    int alphaOrig=a;
    Move bestMove{};
    int moveIndex=0;
    int side=p.side()==1?0:1;

    for(const auto&m:mv){
        int moved=p.at(m.from);
        int captured=isCapture(m)?p.at(m.to):0;
        if(m.flag==Flag::EnPassant)
            captured=p.at(m.to+(moved>0?-8:8));

        const bool quiet=!isCapture(m)&&!isPromotion(m);

        // Late quiet pruning.
        if(d<=3 && quiet && !check && moveIndex>2 && staticEval+80*d<=a)
            continue;

        // More selective SEE pruning for late captures.
        if(d>=2 && isCapture(m) && moveIndex>6 && seeCapture(p,m)<-70*d)
            continue;

        if(!p.make(m)) continue;
        ++moveIndex;

        NNUE::Accumulator child=acc;
        network().update(p,m,moved,captured,m.flag==Flag::EnPassant,child);

        const bool givesCheck=p.inCheck(p.side()==1);
        int searchDepth=d-1+(givesCheck?1:0);

        int reduction=0;
        if(d>=4 && moveIndex>4 && quiet && !check){
            reduction=1;
            if(d>=7 && moveIndex>10) reduction=2;
        }

        int s;
        if(reduction){
            s=-negamax(p,child,searchDepth-reduction,-a-1,-a,ply+1);
            if(s>a)
                s=-negamax(p,child,searchDepth,-b,-a,ply+1);
        } else if(moveIndex==1){
            s=-negamax(p,child,searchDepth,-b,-a,ply+1);
        } else {
            s=-negamax(p,child,searchDepth,-a-1,-a,ply+1);
            if(s>a && s<b)
                s=-negamax(p,child,searchDepth,-b,-a,ply+1);
        }

        p.undo();

        if(timeUp()) return 0;

        if(s>best){
            best=s;
            bestMove=m;
        }

        if(s>a){
            a=s;
            if(quiet) updateHistory(side,m,d,+1);
        } else if(quiet){
            updateHistory(side,m,d,-1);
        }

        if(a>=b){
            if(quiet && ply<MAX_PLY && !(m==killers1_[ply])){
                killers2_[ply]=killers1_[ply];
                killers1_[ply]=m;
            }
            break;
        }
    }

    Bound bd=best<=alphaOrig ? Bound::Upper :
             (best>=b ? Bound::Lower : Bound::Exact);

    tt_.store(key,d,best,bd,bestMove);
    return best;
}

int Search::rootSearch(Position&p,NNUE::Accumulator& acc,int d,int alpha,int beta,Move&bm){
    auto mv=p.legal();
    if(mv.empty()){
        bm={};
        return p.inCheck(p.side()==1)?-MATE:0;
    }

    uint64_t rootKey=hash(p);
    int ignored=0;
    Move ttMove{};
    tt_.probe(rootKey,d,alpha,beta,ignored,ttMove);

    std::stable_sort(mv.begin(),mv.end(),[&](const Move&x,const Move&y){
        return moveScore(p,x,0,ttMove)==moveScore(p,y,0,ttMove)
            ? (x.from*64+x.to)<(y.from*64+y.to)
            : moveScore(p,x,0,ttMove)>moveScore(p,y,0,ttMove);
    });

    int best=-INF;
    bool first=true;

    for(const auto&m:mv){
        int moved=p.at(m.from);
        int captured=isCapture(m)?p.at(m.to):0;
        if(m.flag==Flag::EnPassant)
            captured=p.at(m.to+(moved>0?-8:8));

        if(!p.make(m)) continue;

        NNUE::Accumulator child=acc;
        network().update(p,m,moved,captured,m.flag==Flag::EnPassant,child);

        const bool givesCheck=p.inCheck(p.side()==1);
        const int childDepth=d-1+(givesCheck?1:0);

        int s;
        if(first){
            s=-negamax(p,child,childDepth,-beta,-alpha,1);
            first=false;
        }else{
            s=-negamax(p,child,childDepth,-alpha-1,-alpha,1);
            if(s>alpha&&s<beta)
                s=-negamax(p,child,childDepth,-beta,-alpha,1);
        }

        p.undo();

        if(timeUp())return 0;

        if(s>best){
            best=s;
            bm=m;
        }
        if(s>alpha)alpha=s;
        if(alpha>=beta)break;
    }

    tt_.store(rootKey,d,best,
              best<=alpha ? Bound::Upper : (best>=beta ? Bound::Lower : Bound::Exact),
              bm);
    return best;
}

int Search::rootSearchParallel(Position&p,NNUE::Accumulator&acc,int d,int alpha,int beta,Move&bm){
    auto mv=p.legal();
    if(mv.empty()){
        bm={};
        return p.inCheck(p.side()==1)?-MATE:0;
    }

    std::stable_sort(mv.begin(),mv.end(),[&](const Move&x,const Move&y){
        return moveScore(p,x,0,pvMove_)>moveScore(p,y,0,pvMove_);
    });

    if(threads_<=1||mv.size()<=1)
        return rootSearch(p,acc,d,alpha,beta,bm);

    struct Result{Move m{};int score=-INF;bool done=false;};
    std::vector<Result> results(mv.size());
    std::atomic<size_t> next{0};
    std::vector<std::thread> workers;

    auto deadline=deadline_;
    bool timed=timed_;
    int n=std::min<int>(threads_,static_cast<int>(mv.size()));
    workers.reserve(n);

    for(int t=0;t<n;t++){
        workers.emplace_back([&,deadline,timed](){
            Search worker(32);
            worker.setThreads(1);
            worker.externalStop_=&stop_;
            worker.stop_=false;
            worker.timed_=timed;
            worker.deadline_=deadline;
            worker.pvMove_=pvMove_;

            while(!worker.timeUp()){
                size_t i=next.fetch_add(1);
                if(i>=mv.size())break;

                Position child=p;

                int moved=p.at(mv[i].from);
                int captured=isCapture(mv[i])?p.at(mv[i].to):0;
                if(mv[i].flag==Flag::EnPassant)
                    captured=p.at(mv[i].to+(moved>0?-8:8));

                if(!child.make(mv[i])) continue;

                NNUE::Accumulator ca=acc;
                network().update(p,mv[i],moved,captured,mv[i].flag==Flag::EnPassant,ca);

                int s=-worker.negamax(child,ca,d-1,-beta,-alpha,1);
                results[i]={mv[i],s,true};

                nodes_.fetch_add(worker.nodes_.load(std::memory_order_relaxed),
                                 std::memory_order_relaxed);
                worker.nodes_=0;
            }
        });
    }

    for(auto&th:workers)th.join();

    int best=-INF;
    Move bestM=mv.front();
    bool any=false;

    for(const auto&r:results){
        if(!r.done)continue;
        any=true;
        if(r.score>best){
            best=r.score;
            bestM=r.m;
        }
    }

    if(!any)return 0;
    bm=bestM;
    return best;
}

Move Search::searchInternal(Position&p,int depth,int movetimeMs){
    stop_=false;
    nodes_=0;
    score_=0;
    completedDepth_=0;
    rootBest_={};
    pvMove_={};

    timed_=movetimeMs>0;
    if(timed_)
        deadline_=std::chrono::steady_clock::now()+std::chrono::milliseconds(movetimeMs);

    depth=std::clamp(depth,1,64);

    auto legal=p.legal();
    if(legal.empty())return{};

    rootBest_=legal.front();

    NNUE::Accumulator rootAcc;
    network().refresh(p,rootAcc);

    int previous=0;

    // Iterative deepening with aspiration-window widening.
    for(int d=1;d<=depth;d++){
        int alpha=-INF,beta=INF;
        int window=(d>=4?24:INF);

        if(d>=4){
            alpha=std::max(-INF,previous-window);
            beta=std::min(INF,previous+window);
        }

        Move bm{};
        int s=0;

        while(true){
            s=(threads_>1
                ? rootSearchParallel(p,rootAcc,d,alpha,beta,bm)
                : rootSearch(p,rootAcc,d,alpha,beta,bm));

            if(timeUp())break;

            if(d<4 || (s>alpha && s<beta))
                break;

            // Widen gradually rather than jumping straight to full window.
            window*=2;
            if(s<=alpha)
                alpha=std::max(-INF,s-window);
            else
                beta=std::min(INF,s+window);

            if(alpha<=-INF && beta>=INF){
                s=(threads_>1
                    ? rootSearchParallel(p,rootAcc,d,-INF,INF,bm)
                    : rootSearch(p,rootAcc,d,-INF,INF,bm));
                break;
            }
        }

        if(timeUp())break;

        previous=s;
        score_=s;
        rootBest_=bm;
        pvMove_=bm;
        completedDepth_=d;
    }

    timed_=false;
    return rootBest_;
}

Move Search::go(Position&p,int depth){
    return searchInternal(p,depth,0);
}

Move Search::go(Position&p,int depth,int movetimeMs){
    return searchInternal(p,depth,movetimeMs);
}

Move Search::goTimed(Position&p,int maxDepth,int wtimeMs,int btimeMs,int wincMs,int bincMs,int movesToGo){
    int remain=p.side()==1?wtimeMs:btimeMs;
    int inc=p.side()==1?wincMs:bincMs;
    int mt=0;

    if(remain>0){
        int divisor=movesToGo>0?movesToGo:30;
        // A slightly more aggressive Android time allocation with plenty of
        // reserve for tactical positions.
        mt=remain/divisor+inc*3/4;
        mt=std::clamp(mt,10,std::max(10,remain));
    }

    return searchInternal(p,std::clamp(maxDepth,1,64),mt);
}

}
