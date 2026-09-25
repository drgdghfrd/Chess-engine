#include "Search.h"
#ifdef CHESSZERO_FATHOM
#include "../tablebase/Syzygy.h"
#endif
#include "Evaluation.h"
#include "../chess/Move.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <thread>
#include <string>

namespace uc {

static constexpr int INF=30000;
static constexpr int MATE=29000;
static constexpr int TB_WIN=28000;
#ifdef CHESSZERO_FATHOM
static constexpr int TB_CURSED=27000;
static int tbScore(SyzygyWDL wdl) { switch(wdl){ case SyzygyWDL::Win:return TB_WIN; case SyzygyWDL::CursedWin:return TB_CURSED; case SyzygyWDL::BlessedWin:return -TB_CURSED; case SyzygyWDL::Loss:return -TB_WIN; case SyzygyWDL::Draw:return 0; default:return 0; } }
#endif


Search::Search(size_t ttMB):tt_(ttMB), configuredHashMB_(ttMB), appliedThreads_(1), appliedHashMB_(ttMB){}

Search::Search(size_t ttMB, TranspositionTable* sharedTT):tt_(ttMB), sharedTT_(sharedTT), configuredHashMB_(ttMB), appliedThreads_(1), appliedHashMB_(ttMB){
    if (sharedTT_) sharedTT_->setThreadSafe(true);
}

std::vector<size_t> Search::splitHashBudget(size_t totalMB, size_t contexts) {
    if (contexts == 0) return {};
    totalMB = std::max<size_t>(1, totalMB);

    // TT buckets are powers of two, so each slice is chosen from powers of two
    // to avoid hidden 1.5x–2x allocator rounding. The first slot is the root
    // coordinator; any leftover whole power-of-two increment is given there.
    std::vector<size_t> shares(contexts, 0);
    if (totalMB < contexts) {
        for (size_t i = 0; i < totalMB && i < contexts; ++i) shares[i] = 1;
        return shares;
    }

    size_t base = 1;
    const size_t target = totalMB / contexts;
    while (base <= target / 2) base *= 2;
    std::fill(shares.begin(), shares.end(), base);
    size_t used = base * contexts;
    size_t remaining = totalMB - used;

    // Distribute every affordable power-of-two upgrade across contexts in
    // order. This keeps slices balanced while giving the coordinator the first
    // opportunity to receive a larger slice. Example: 128 MB / 5 contexts
    // becomes 32,32,32,16,16 rather than concentrating 64 MB in one worker.
    for (;;) {
        bool upgraded = false;
        for (size_t i = 0; i < shares.size(); ++i) {
            const size_t cost = shares[i];
            if (cost == 0 || cost > remaining) continue;
            shares[i] *= 2;
            remaining -= cost;
            upgraded = true;
        }
        if (!upgraded) break;
    }
    return shares;
}

void Search::applyParallelHashLayout(){
    const int requestedThreads = std::clamp(hot_.threads, 1, 64);
    if (sharedTT_) {
        if (appliedThreads_ == requestedThreads && appliedHashMB_ == configuredHashMB_) return;
        tt().resize(std::max<size_t>(1, configuredHashMB_));
        appliedThreads_ = requestedThreads;
        appliedHashMB_ = configuredHashMB_;
        return;
    }

    if (appliedThreads_ == requestedThreads && appliedHashMB_ == configuredHashMB_) return;

    const size_t contexts = static_cast<size_t>(requestedThreads);
    const auto shares = splitHashBudget(configuredHashMB_, contexts);

    if (contexts <= 1) {
        parallelWorkers_.clear();
        tt_.resize(shares.empty() ? configuredHashMB_ : shares[0]);
        appliedThreads_ = requestedThreads;
        appliedHashMB_ = configuredHashMB_;
        return;
    }

    const size_t workerCount = contexts - 1;
    while (parallelWorkers_.size() < workerCount) {
        const size_t index = parallelWorkers_.size() + 1;
        auto worker = std::make_unique<Search>(index < shares.size() ? shares[index] : 0);
        worker->hot_.threads = 1;
        parallelWorkers_.push_back(std::move(worker));
    }
    while (parallelWorkers_.size() > workerCount)
        parallelWorkers_.pop_back();

    tt_.resize(shares.empty() ? 0 : shares[0]);
    for (size_t i = 0; i < parallelWorkers_.size(); ++i) {
        auto& worker = parallelWorkers_[i];
        const size_t slice = (i + 1 < shares.size()) ? shares[i + 1] : 0;
        worker->configuredHashMB_ = slice;
        worker->appliedThreads_ = 1;
        worker->appliedHashMB_ = slice;
        worker->tt_.resize(slice);
    }
    appliedThreads_ = requestedThreads;
    appliedHashMB_ = configuredHashMB_;
}

void Search::setThreads(int n){
    hot_.threads = std::clamp(n, 1, 64);
    // UCI and EngineAPI serialize configuration changes outside active search.
    // Apply immediately so switching 7→5 threads really changes the worker pool
    // before the next go/isready cycle.
    applyParallelHashLayout();
}

void Search::setHashMB(size_t mb){
    configuredHashMB_ = std::max<size_t>(1, mb);
    applyParallelHashLayout();
}

size_t Search::allocatedHashMB() const {
    const size_t bytes = tt().size() * sizeof(TTEntry);
    return bytes / (1024ULL * 1024ULL);
}

size_t Search::totalParallelHashMB() const {
    size_t total = allocatedHashMB();
    for (const auto& worker : parallelWorkers_) total += worker->allocatedHashMB();
    return total;
}

std::vector<size_t> Search::hashSlicesMB() const {
    std::vector<size_t> slices;
    slices.reserve(parallelWorkers_.size() + 1);
    slices.push_back(allocatedHashMB());
    for (const auto& worker : parallelWorkers_)
        slices.push_back(worker->allocatedHashMB());
    return slices;
}

void Search::clearHash(){
    tt().clear();
    for (auto& worker : parallelWorkers_) worker->tt().clear();
}

void Search::prepareParallelWorkers(){
    if (hot_.threads <= 1){
        return;
    }

    // The worker layout is applied by setThreads()/setHashMB(). Re-applying here
    // is cheap when the configuration is already current and also protects direct
    // Search users that modify hot_ state before a search begins.
    applyParallelHashLayout();

    for (auto& worker : parallelWorkers_){
        worker->stop_.store(false, std::memory_order_relaxed);
        worker->externalStop_ = &stop_;
        worker->hot_ = Hot{};
        worker->hot_.threads = 1;
        worker->syzygy_ = syzygy_;
        worker->nodes_.store(0, std::memory_order_relaxed);
        worker->tt().clear();
    }
}

void Search::stop(){stop_=true;}

bool Search::timeUp(){
    if(stop_) return true;
    if(externalStop_ && externalStop_->load(std::memory_order_relaxed)) return true;
    if(!hot_.timed) return false;

    // Keep the hard bound responsive enough for blitz/mobile search.  The old
    // 2048-node polling interval could overshoot a short clock budget by a
    // noticeable amount on tactical positions.  Poll more often without
    // asking steady_clock for every single node.
    const uint64_t n = nodes_.load(std::memory_order_relaxed);
    constexpr uint64_t kNormalPoll = 256;
    if ((n & (kNormalPoll - 1)) == 0) {
        if (std::chrono::steady_clock::now() >= hardDeadline_) {
            stop_.store(true, std::memory_order_relaxed);
            return true;
        }
    }
    return false;
}

bool Search::softTimeUp() const {
    if(!hot_.timed || !hot_.useSoft) return false;
    return std::chrono::steady_clock::now() >= softDeadline_;
}

uint64_t Search::hash(const Position&p)const{
    // Incremental Zobrist key maintained by Position::make/undo
    return p.key();
}

int Search::pieceValue(int p)const{
    static const int v[7]={0,100,320,330,500,900,20000};
    return v[std::abs(p)];
}

int Search::moveScore(const Position&p,const Move&m,int ply,const Move&ttMove,const Move&prev)const{
    if(m==ttMove) return 3000000;

    int s=0;
    int attacker=pieceValue(p.at(m.from));
    int victim=0;
    if(isCapture(m)){
        if(m.flag==Flag::EnPassant) victim=100;
        else victim=pieceValue(p.at(m.to));
        s += 1000000 + 16*victim - attacker;
    }
    if(isPromotion(m))
        s += 800000 + pieceValue(promotionPiece(m))*16;

    if(ply<MAX_PLY){
        if(m==hot_.killers1[ply]) s+=650000;
        else if(m==hot_.killers2[ply]) s+=600000;
    }

    // Countermove: quiet reply that previously caused a beta cutoff after prev
    if (hot_.countermove && prev.from < 64 && prev.to < 64 &&
        m == hot_.counterMove[prev.from][prev.to]) {
        s += 550000 * hot_.countermoveAggression / 100;
        ++hot_.countermoveHits;
    }

    int side=p.side()==1?0:1;
    const int hist = hot_.history[side][m.from][m.to];
    const int historyScale = hot_.historyEnabled ? hot_.historyAggression : 0;
    s += hist * historyScale / 100;

    // v0.51 Continuation History: previous-move context.
    const int cont = continuationScore(p, prev, m);
    s += cont * historyScale / 100;

    // v0.50 Countermove History: a bounded contextual hint weaker than the
    // exact countermove hit above.
    if (hot_.countermove && prev.from < 64 && prev.to < 64)
        s += static_cast<int>(hot_.counterHistory[prev.from][prev.to][m.to]) * 4 * hot_.countermoveAggression / 100;

    // v0.52: combine independent quiet-move heuristics so a move which is
    // consistently good in several contexts rises above noisy candidates.
    if (historyScale > 0 && !isCapture(m) && !isPromotion(m))
        s += std::clamp((hist / 8 + cont / 8) * historyScale / 100, -8000, 8000);

    return s;
}

void Search::updateHistory(int side,const Move&m,int depth,int bonusSign){
    int &h=hot_.history[side][m.from][m.to];
    int delta=std::clamp(depth*depth*bonusSign*12,-MAX_HISTORY,MAX_HISTORY);
    h += delta;
    h -= h*std::abs(delta)/(MAX_HISTORY+1);
    h=std::clamp(h,-MAX_HISTORY,MAX_HISTORY);
}

void Search::updateContHistory(const Move& prev, const Move& m, int depth, int bonusSign){
    if (prev.to >= 64 || m.to >= 64) return;
    int16_t &h = hot_.contHistory[prev.to][m.to];
    constexpr int maxH = 8191;
    int delta = std::clamp(depth * depth * bonusSign * 10, -maxH, maxH);
    int v = static_cast<int>(h) + delta;
    v -= v * std::abs(delta) / (maxH + 1);
    h = static_cast<int16_t>(std::clamp(v, -maxH, maxH));
}

int Search::continuationScore(const Position& p, const Move& prev, const Move& m) const {
    if (prev.from >= 64 || prev.to >= 64 || m.to >= 64) return 0;

    int score = static_cast<int>(hot_.contHistory[prev.to][m.to]);
    int previousPiece = p.at(prev.to);
    // During move ordering the previous mover is still on prev.to. During
    // heuristic updates the parent position has been restored, so it is back
    // on prev.from. Support both contexts without extra state.
    if (previousPiece == EMPTY && prev.from < 64)
        previousPiece = p.at(prev.from);
    const int pieceType = std::abs(previousPiece);
    if (pieceType >= 1 && pieceType <= 6)
        score += static_cast<int>(hot_.contPieceHistory[pieceType - 1][prev.to][m.to]) * 2;
    return score;
}

void Search::updateContPieceHistory(const Position& p, const Move& prev, const Move& m, int depth, int bonusSign){
    if (prev.from >= 64 || prev.to >= 64 || m.to >= 64) return;

    int previousPiece = p.at(prev.to);
    // During move ordering the previous mover is still on prev.to. During
    // heuristic updates the parent position has been restored, so it is back
    // on prev.from. Support both contexts without extra state.
    if (previousPiece == EMPTY && prev.from < 64)
        previousPiece = p.at(prev.from);
    const int pieceType = std::abs(previousPiece);
    if (pieceType < 1 || pieceType > 6) return;

    int16_t &h = hot_.contPieceHistory[pieceType - 1][prev.to][m.to];
    constexpr int maxH = 8191;
    int delta = std::clamp(depth * depth * bonusSign * 8, -maxH, maxH);
    int v = static_cast<int>(h) + delta;
    v -= v * std::abs(delta) / (maxH + 1);
    h = static_cast<int16_t>(std::clamp(v, -maxH, maxH));
}

void Search::updateCounterHistory(const Move& prev, const Move& m, int depth, int bonusSign){
    if (prev.from >= 64 || prev.to >= 64 || m.to >= 64) return;
    int16_t &h = hot_.counterHistory[prev.from][prev.to][m.to];
    const int maxH = 8191;
    int delta = std::clamp(depth * depth * bonusSign * 8, -maxH, maxH);
    int v = static_cast<int>(h);
    v += delta;
    v -= v * std::abs(delta) / (maxH + 1);
    h = static_cast<int16_t>(std::clamp(v, -maxH, maxH));
}



void Search::updatePV(int ply, const Move& m) {
    if (ply < 0 || ply >= MAX_PLY) return;
    hot_.pv[ply][0] = m;
    int next = (ply + 1 < MAX_PLY) ? hot_.pvLen[ply + 1] : 0;
    if (next > MAX_PV - 1) next = MAX_PV - 1;
    for (int i = 0; i < next; ++i)
        hot_.pv[ply][i + 1] = hot_.pv[ply + 1][i];
    hot_.pvLen[ply] = next + 1;
}

std::string Search::pvString() const {
    std::string s;
    int n = hot_.pvLen[0];
    for (int i = 0; i < n; ++i) {
        if (i) s += ' ';
        s += toUci(hot_.pv[0][i]);
    }
    return s;
}

int Search::qsearch(Position&p,NNUE::Accumulator& acc,int a,int b,int ply){
    if(timeUp()) return 0;
    if(p.isDraw()) return 0;
    ++nodes_;

    // v0.56: keep the tactical horizon bounded even when a checking sequence
    // refuses to settle.  In-check nodes are still allowed to search every
    // legal evasion until this safety ceiling.
    constexpr int QMAX_PLY = 32;
    if(ply >= QMAX_PLY)
        return evaluate(p,acc);

    const bool check=p.inCheck(p.side()==1);
    int stand=evaluate(p,acc);

    // Stand-pat is illegal while in check.
    if(!check){
        if(stand>=b) return b;
        if(stand>a) a=stand;
    }

    std::vector<Move>& mv = moveList_[std::min(ply, MAX_PLY - 1)];
    p.generateLegal(mv);
    std::sort(mv.begin(),mv.end(),[&](const Move&x,const Move&y){
        // v0.56: SEE-aware tactical ordering.  MVV-LVA remains the fallback
        // for quiet/check moves, while good exchanges rise above bad ones.
        auto qscore = [&](const Move&m){
            int s=moveScore(p,m,ply,Move{},Move{});
            if(isCapture(m)){
                const int see=seeCapture(p,m);
                s += std::clamp(see * 32, -20000, 20000);
            }
            if(isPromotion(m)) s += 12000;
            return s;
        };
        return qscore(x)>qscore(y);
    });

    for(const auto&m:mv){
        // In check, every legal evasion must be searched. Outside check,
        // qsearch keeps captures/promotions and bounded quiet checks.
        const bool quietCandidate = !isCapture(m) && !isPromotion(m);
        if(!check && quietCandidate && ply >= 16) continue;

        // v0.56 delta pruning: if even a generous material swing cannot
        // reach alpha, skip the capture. Promotions and checks are tactical
        // exceptions and are never removed by this rule.
        if(!check && isCapture(m) && !isPromotion(m)){
            int gain=(m.flag==Flag::EnPassant)?100:pieceValue(p.at(m.to));
            if(stand + gain + 120 < a) continue;

            // SEE pruning rejects clearly losing captures while retaining a
            // safety margin for tactical compensation.
            const int see=seeCapture(p,m);
            if(see < -120 && stand + gain + 80 < a) continue;
        }

        int moved=p.at(m.from);
        int captured=isCapture(m)?p.at(m.to):0;
        if(m.flag==Flag::EnPassant)
            captured=p.at(m.to+(moved>0?-8:8));

        if(!p.make(m)) continue;
        const int toPiece = p.at(m.to);
        const bool ep = (m.flag==Flag::EnPassant);
        network().applyMoveFeatures(p, acc, m, moved, captured, ep, toPiece, +1);
        tt().prefetch(hash(p));

        const bool givesCheck = p.inCheck(p.side()==1);
        if (quietCandidate && !givesCheck) {
            network().applyMoveFeatures(p, acc, m, moved, captured, ep, toPiece, -1);
            p.undo();
            continue;
        }

        int s=-qsearch(p,acc,-b,-a,ply+1);

        network().applyMoveFeatures(p, acc, m, moved, captured, ep, toPiece, -1);
        p.undo();

        if(timeUp()) return 0;
        if(s>=b) return b;
        if(s>a) a=s;
    }
    return a;
}

// v1.0.9 SEE: bitboard "Swap Algorithm" (chessprogramming.org/SEE_-_The_Swap_Algorithm).
// Replaces the v0.14 make/unmake + legal-move-gen loop with pure bitboard operations:
// no Position copy, no move generation, no heap allocation. Uses the new magic-bitboard
// slider lookups plus x-ray discovery. Used only for capture ordering and pruning.
int Search::seeCapture(const Position&p,const Move&m) const{
    if(!isCapture(m)) return 0;
    static const int V[7]={0,100,320,330,500,900,20000}; // index by abs(piece)
    const int from=m.from, to=m.to;
    const int movedPiece=std::abs(p.at(from));
    // First capture: gain[0] = value of captured piece (+ promotion upgrade).
    int capturedType;
    if(m.flag==Flag::EnPassant) capturedType=1;
    else capturedType=std::abs(p.at(to));
    int gain[32];
    gain[0]=V[capturedType];
    int attackerType=movedPiece; // piece that now sits on `to` (captured next if recaptured)
    if(isPromotion(m)){
        const int promo=promotionPiece(m);
        gain[0]+=V[promo]-V[movedPiece];
        attackerType=promo;
    }
    const Bitboards& bb=p.bitboards();
    Bitboard occ=bb.white|bb.black;
    occ&=~(1ULL<<from);
    occ&=~(1ULL<<to);
    if(m.flag==Flag::EnPassant){
        const int capSq=to+(p.at(from)>0?-8:8);
        occ&=~(1ULL<<capSq);
    }
    // side: 0 = white, 1 = black. The first mover's side; the other side recaptures first.
    int side=(p.at(from)>0)?0:1;
    side^=1;
    // All pieces attacking `to` given occupancy occ (recomputed; x-rays added per removal).
    auto attacksTo=[&](Bitboard occ2)->Bitboard{
        Bitboard r=0;
        r|=BitOps::pawn(to,false)&bb.piece[0];            // white pawns attack to (south diagonals)
        r|=BitOps::pawn(to,true)&bb.piece[6];             // black pawns attack to (north diagonals)
        r|=BitOps::knight(to)&(bb.piece[1]|bb.piece[7]);  // knights
        r|=BitOps::king(to)&(bb.piece[5]|bb.piece[11]);   // kings
        const Bitboard diag=BitOps::bishop(to,occ2);
        r|=diag&(bb.piece[2]|bb.piece[8]|bb.piece[4]|bb.piece[10]); // bishops+queens
        const Bitboard orth=BitOps::rook(to,occ2);
        r|=orth&(bb.piece[3]|bb.piece[9]|bb.piece[4]|bb.piece[10]); // rooks+queens
        return r;
    };
    Bitboard attackers=attacksTo(occ);
    const Bitboard diagPieces=bb.piece[2]|bb.piece[8]|bb.piece[4]|bb.piece[10];
    const Bitboard orthPieces=bb.piece[3]|bb.piece[9]|bb.piece[4]|bb.piece[10];
    int d=0;
    while(true){
        const Bitboard mine=attackers&occ&(side==0?bb.white:bb.black);
        if(!mine) break;
        // Least valuable attacker: pawn(1) -> knight(2) -> bishop(3) -> rook(4) -> queen(5) -> king(6).
        // piece index within a color: 0=pawn,1=knight,2=bishop,3=rook,4=queen,5=king.
        int pt=-1, sq=-1;
        for(int t=0;t<6;++t){
            const Bitboard cand=mine&bb.piece[side*6+t];
            if(cand){ pt=t+1; sq=BitOps::lsb(cand); break; }
        }
        if(pt<0) break;
        ++d;
        gain[d]=V[attackerType]-gain[d-1]; // previous attacker is the piece being captured now
        attackerType=pt;
        occ&=~(1ULL<<sq); // remove this attacker; sliders behind it now see `to`
        attackers|=BitOps::bishop(to,occ)&diagPieces;
        attackers|=BitOps::rook(to,occ)&orthPieces;
        side^=1;
    }
    // Negamax backup: each side decides whether the recapture is worth it.
    while(d){
        gain[d-1]=-std::max(-gain[d-1],gain[d]);
        --d;
    }
    return gain[0];
}

int Search::negamax(Position&p,NNUE::Accumulator& acc,int d,int a,int b,int ply,const Move& prev,bool allowNull,const Move& excluded){
    if(timeUp()) return 0;
    if(p.isDraw()) return 0;
    ++nodes_;
    if (ply < MAX_PLY) hot_.pvLen[ply] = 0;

    const bool check=p.inCheck(p.side()==1);
    if(d<=0) return qsearch(p,acc,a,b,ply);

#ifdef CHESSZERO_FATHOM
    if (syzygy_) {
        const SyzygyProbeResult tb = syzygy_->probe(p);
        if (tb.wdlAvailable) {
            const int score = tbScore(tb.wdl);
            return score;
        }
    }
#endif

    uint64_t key=hash(p);
    int ttScore=0;
    Move ttMove{};
    bool ttHitMove=false;
    if(tt().probe(key,d,a,b,ply,ttScore,ttMove,ttHitMove)) return ttScore;

    int staticEval=evaluate(p,acc);

    // Mate distance boundaries.
    a=std::max(a,-MATE+ply);
    b=std::min(b,MATE-ply);
    if(a>=b) return a;

    // Reverse futility / static null pruning (fail-high on high eval).
    // Tuned: slightly wider at d=2/3 to cut more safe fail-highs.
    if (d <= 3 && !check && std::abs(staticEval) < TB_WIN) {
        static const int rfpMargin[] = {0, 90, 190, 280};
        if (staticEval - rfpMargin[d] >= b)
            return staticEval;
    }

    // v0.79 Razoring: configurable, conservative near the horizon.
    // Only a qsearch fail-low is allowed to prune; a qsearch fail-high falls
    // through to the normal full-width search. 100 aggression = v0.78 margin.
    if (hot_.razoring && d <= 3 && !check && std::abs(staticEval) < TB_WIN && !ttHitMove) {
        static const int razorBase[] = {0, 200, 400, 600};
        const int margin = razorBase[d] * hot_.razorAggression / 100;
        if (staticEval + margin < a) {
            ++hot_.razorSearches;
            int r = qsearch(p, acc, a, b, ply);
            if (r < a) {
                ++hot_.razorFailLows;
                return r;
            }
        }
    }

    // v0.79 Null move pruning: expose the main safety/aggression knobs while
    // keeping the v0.78 baseline at aggression=100. Higher aggression widens
    // the eval gate and may add one reduction at deeper nodes; lower aggression
    // does the opposite. Verification remains enabled by default at d>=8.
    if (hot_.nullMove && d >= 3 && !check && allowNull
        && std::abs(b) < TB_WIN && std::abs(staticEval) < TB_WIN) {
        auto bb = p.bitboards();
        // Minors + majors (exclude pawns and kings): indices 1..4 and 7..10
        uint64_t nonPawns =
            (bb.piece[1] | bb.piece[2] | bb.piece[3] | bb.piece[4] |
             bb.piece[7] | bb.piece[8] | bb.piece[9] | bb.piece[10]);
        // At least one non-pawn piece total (both colours) — crude zugzwang guard
        int npCount = 0;
        uint64_t tmp = nonPawns;
        while (tmp) { ++npCount; tmp &= tmp - 1; }

        const int evalGate = (40 + 5 * d) * hot_.nullMoveAggression / 100;
        if (npCount >= 1 && staticEval >= b - evalGate) {
            int R = 2 + d / 6;
            if (hot_.nullMoveAggression >= 125 && d >= 8) ++R;
            if (hot_.nullMoveAggression <= 75 && d >= 6) --R;
            if (R < 2) R = 2;
            if (R > 4) R = 4;
            if (R > d - 1) R = d - 1;

            ++hot_.nullMoveSearches;
            p.makeNull();
            // Null move does not change pieces → reuse accumulator (big mobile win)
            // Null move: pieces unchanged — reuse same accumulator (no copy)
            tt().prefetch(hash(p));
            int ns = -negamax(p, acc, d - 1 - R, -b, -b + 1, ply + 1, Move{}, false);
            p.undoNull();

            if (timeUp()) return 0;

            if (ns >= b) {
                ++hot_.nullMoveCutoffs;
                if (hot_.nullMoveVerification && d >= 8) {
                    ++hot_.nullMoveVerifications;
                    // Verification: re-search without null at depth d-R-1
                    int vDepth = d - R - 1;
                    if (vDepth < 1) vDepth = 1;
                    // Verification at same position — reuse accumulator
                    int verify = -negamax(p, acc, vDepth, -b, -b + 1, ply + 1, Move{}, false);
                    if (timeUp()) return 0;
                    if (verify >= b) {
                        tt().store(key, d, verify, Bound::Lower, ply, ttMove);
                        return verify;
                    }
                    ++hot_.nullMoveVerificationFailures;
                    // verification failed → fall through to full search
                } else {
                    tt().store(key, d, ns, Bound::Lower, ply, ttMove);
                    return ns;
                }
            }
        }
    }

    // ProbCut (tuned): shallow probe vs raised beta.
    // - depth >= 4 (was 5): more opportunities in mid-tree
    // - only if staticEval is near/above beta (otherwise probe is wasted)
    // - adaptive reduction R = 3 + depth/4, margin grows mildly with depth
    // - skip near mate scores
    if (d >= 4 && !check && allowNull
        && std::abs(b) < TB_WIN && std::abs(staticEval) < TB_WIN
        && staticEval >= b - 120) {
        // margin: ~90 at d=4, ~120 at d=8, ~150 at d=12
        const int pcMargin = 60 + 8 * d;
        const int rbeta = b + pcMargin;
        // reduction: keep probe clearly shallower than full search
        int R = 3 + d / 4;
        if (R > d - 1) R = d - 1;
        int rdepth = d - R;
        if (rdepth < 1) rdepth = 1;

        int pc = negamax(p, acc, rdepth, rbeta - 1, rbeta, ply, prev, false);
        if (timeUp()) return 0;
        if (pc >= rbeta) {
            tt().store(key, d, pc, Bound::Lower, ply, ttMove);
            return pc;
        }
    }

    std::vector<Move>& mv = moveList_[std::min(ply, MAX_PLY - 1)];
    p.generateLegal(mv);
    if(mv.empty()) return check ? -MATE+ply : 0;

    std::sort(mv.begin(),mv.end(),[&](const Move&x,const Move&y){
        return moveScore(p,x,ply,ttMove,prev)>moveScore(p,y,ply,ttMove,prev);
    });

    // --- Singular extension (v0.37) ---
    // If TT move is clearly better than all others at reduced depth, extend it.
    int singularExt = 0;
    if (d >= 7 && ttHitMove && allowNull && !check
        && std::abs(ttScore) < TB_WIN
        && std::abs(a) < TB_WIN && std::abs(b) < TB_WIN
        && ply + 1 < MAX_PLY) {
        // Confirm ttMove is legal (in list)
        bool ttLegal = false;
        for (const auto& xm : mv) {
            if (xm == ttMove) { ttLegal = true; break; }
        }
        if (ttLegal) {
            const int margin = 40 + 2 * d;
            const int sBeta = ttScore - margin;
            int sDepth = (d - 1) / 2;
            if (sDepth < 1) sDepth = 1;
            // Reduced search excluding the TT move
            int sScore = negamax(p, acc, sDepth, sBeta - 1, sBeta, ply, prev,
                                 false, ttMove);
            if (timeUp()) return 0;
            if (sScore < sBeta)
                singularExt = 1;  // others cannot reach TT score band
        }
    }

    int best=-INF;
    int alphaOrig=a;
    Move bestMove{};
    int moveIndex=0;
    int side=p.side()==1?0:1;

    for(const auto&m:mv){
        // Skip excluded move (used by singular verification search)
        if (m == excluded) continue;
        int moved=p.at(m.from);
        int captured=isCapture(m)?p.at(m.to):0;
        if(m.flag==Flag::EnPassant)
            captured=p.at(m.to+(moved>0?-8:8));

        const bool quiet=!isCapture(m)&&!isPromotion(m);

        const bool isTT = (m == ttMove);
        const bool isKiller = (ply < MAX_PLY &&
                               (m == hot_.killers1[ply] || m == hot_.killers2[ply]));
        const bool isCounter = (prev.from < 64 && prev.to < 64 &&
                                m == hot_.counterMove[prev.from][prev.to]);
        const int hist = hot_.history[side][m.from][m.to];
        const int cont = continuationScore(p, prev, m);

        // v0.52 Heuristic Search: conservative history pruning and late-move
        // pruning. Only strong negative quiet-move evidence is enough to prune.
        if (hot_.historyEnabled && quiet && !check && !isTT && !isKiller && !isCounter) {
            const int heuristic = (hist + cont) * hot_.historyAggression / 100;
            // v0.81: calibrate history-based late-move pruning separately from
            // move ordering. Lower aggression protects more quiet candidates;
            // higher aggression allows stronger pruning of persistently bad moves.
            if (hot_.historyPruning) {
                const int threshold = 5000 * hot_.historyPruningAggression / 100;
                const int lmpLimit = 4 + d * d;
                if (d <= 4 && moveIndex > lmpLimit && heuristic < -threshold) {
                    ++hot_.historyPrunes;
                    continue;
                }
            }

            // Futility pruning for late quiet moves at shallow depth. Keep the
            // baseline margin at 100 and scale only the tunable component.
            if (d <= 4 && moveIndex > 1 && heuristic < -2500) {
                const int margin = 90 + (70 * d * hot_.futilityAggression / 100);
                if (staticEval + margin <= a) {
                    ++hot_.futilityPrunes;
                    continue;
                }
            }
        }

        // SEE pruning for clearly losing late captures. Keep the shallow
        // threshold conservative so tactical shots are not discarded.
        if (d >= 2 && isCapture(m) && moveIndex > 5) {
            const int see = seeCapture(p, m);
            const int margin = 50 + 45 * d;
            if (see < -margin)
                continue;
        }

        if(!p.make(m)) continue;
        ++moveIndex;

        const int toPiece = p.at(m.to);
        const bool ep = (m.flag==Flag::EnPassant);
        network().applyMoveFeatures(p, acc, m, moved, captured, ep, toPiece, +1);
        tt().prefetch(hash(p));

        const bool givesCheck=p.inCheck(p.side()==1);
        int searchDepth=d-1+(givesCheck?1:0);
        // Singular extension: only on the TT move
        if (singularExt && m == ttMove)
            searchDepth += singularExt;

        // v0.78 tuned LMR: reduce late quiet moves more smoothly than the
        // old staircase, while using history/continuation/counter signals to
        // protect moves that have proved useful.  aggression=100 is baseline.
        int reduction=0;
        if (hot_.lmr && d >= 3 && moveIndex >= 3 && quiet && !check && !givesCheck && !isTT) {
            const int combined = hist + cont;
            reduction = 1;
            if (d >= 5 && moveIndex >= 6) ++reduction;
            if (d >= 8 && moveIndex >= 12) ++reduction;
            if (d >= 11 && moveIndex >= 20) ++reduction;

            // A mild depth/move-order boost gives deeper nodes and very late
            // moves extra reduction without introducing floating point math.
            if (hot_.lmrAggression >= 125 && d >= 7 && moveIndex >= 8) ++reduction;
            if (hot_.lmrAggression <= 75 && reduction > 1) --reduction;

            if (combined < -6000) ++reduction;
            else if (combined > 12000) --reduction;
            if (isKiller || isCounter) --reduction;

            reduction = std::clamp(reduction, 0, std::max(0, searchDepth - 1));
            if (reduction > 0) ++hot_.lmrSearches;
        }

        // Clear child PV length before searching child
        if (ply + 1 < MAX_PLY) hot_.pvLen[ply + 1] = 0;

        int s;
        if (moveIndex == 1) {
            // PVS: first move full window
            s = -negamax(p, acc, searchDepth, -b, -a, ply + 1, m);
        } else if (reduction) {
            // Reduced null-window
            s = -negamax(p, acc, searchDepth - reduction, -a - 1, -a, ply + 1, m);
            if (s > a) {
                ++hot_.lmrResearches;
                ++hot_.pvsResearches;
                s = -negamax(p, acc, searchDepth, -b, -a, ply + 1, m);  // research full
            }
        } else {
            // PVS: null window, research on fail-high inside window
            s = -negamax(p, acc, searchDepth, -a - 1, -a, ply + 1, m);
            if (s > a && s < b) {
                ++hot_.pvsResearches;
                s = -negamax(p, acc, searchDepth, -b, -a, ply + 1, m);
            }
        }

        network().applyMoveFeatures(p, acc, m, moved, captured, ep, toPiece, -1);
        p.undo();

        if(timeUp()) return 0;

        if(s>best){
            best=s;
            bestMove=m;
            updatePV(ply, m);
        }

        if(s>a){
            a=s;
            if(quiet) {
                if (hot_.historyEnabled) {
                    updateHistory(side,m,d,+1);
                    updateContHistory(prev, m, d, +1);
                    updateContPieceHistory(p, prev, m, d, +1);
                    ++hot_.historyUpdates;
                }
                if (hot_.countermove) updateCounterHistory(prev, m, d, +1);
            }
        } else if(quiet){
            if (hot_.historyEnabled) {
                updateHistory(side,m,d,-1);
                updateContHistory(prev, m, d,-1);
                updateContPieceHistory(p, prev, m, d,-1);
                ++hot_.historyUpdates;
            }
            if (hot_.countermove) updateCounterHistory(prev, m, d,-1);
        }

        if(a>=b){
            if(quiet && ply<MAX_PLY && !(m==hot_.killers1[ply])){
                hot_.killers2[ply]=hot_.killers1[ply];
                hot_.killers1[ply]=m;
            }
            // Countermove: this quiet caused a cutoff after prev.
            if (hot_.countermove && quiet && prev.from < 64 && prev.to < 64) {
                hot_.counterMove[prev.from][prev.to] = m;
                if (hot_.historyEnabled) {
                    updateContHistory(prev, m, d, +1);
                    updateContPieceHistory(p, prev, m, d, +1);
                }
                updateCounterHistory(prev, m, d, +1);
            }
            break;
        }
    }

    Bound bd=best<=alphaOrig ? Bound::Upper :
             (best>=b ? Bound::Lower : Bound::Exact);

    tt().store(key,d,best,bd,ply,bestMove);
    return best;
}

int Search::rootSearch(Position&p,NNUE::Accumulator& acc,int d,int alpha,int beta,Move&bm){
    std::vector<Move>& mv = moveList_[0];
    p.generateLegal(mv);
    if(mv.empty()){
        bm={};
        return p.inCheck(p.side()==1)?-MATE:0;
    }

    uint64_t rootKey=hash(p);
    int ignored=0;
    Move ttMove{};
    bool ttHitMove=false;
    tt().probe(rootKey,d,alpha,beta,0,ignored,ttMove,ttHitMove);

    // Prefer previous PV move, then TT move for PVS first-move quality
    Move orderMove = hot_.pvMove.from || hot_.pvMove.to ? hot_.pvMove : ttMove;
    std::sort(mv.begin(),mv.end(),[&](const Move&x,const Move&y){
        return moveScore(p,x,0,orderMove,Move{})==moveScore(p,y,0,orderMove,Move{})
            ? (x.from*64+x.to)<(y.from*64+y.to)
            : moveScore(p,x,0,orderMove,Move{})>moveScore(p,y,0,orderMove,Move{});
    });

    int best=-INF;
    const int alphaOrig = alpha;
    bool first=true;

    for(const auto&m:mv){
        int moved=p.at(m.from);
        int captured=isCapture(m)?p.at(m.to):0;
        if(m.flag==Flag::EnPassant)
            captured=p.at(m.to+(moved>0?-8:8));

        if(!p.make(m)) continue;

        const int toPiece = p.at(m.to);
        const bool ep = (m.flag==Flag::EnPassant);
        network().applyMoveFeatures(p, acc, m, moved, captured, ep, toPiece, +1);
        tt().prefetch(hash(p));

        const bool givesCheck=p.inCheck(p.side()==1);
        const int childDepth=d-1+(givesCheck?1:0);

        // Prefer PV move / previous best as first for PVS
        hot_.pvLen[1] = 0;

        int s;
        if(first){
            s=-negamax(p,acc,childDepth,-beta,-alpha,1,m);
            first=false;
        }else{
            s=-negamax(p,acc,childDepth,-alpha-1,-alpha,1,m);
            if(s>alpha&&s<beta)
                s=-negamax(p,acc,childDepth,-beta,-alpha,1,m);
        }

        network().applyMoveFeatures(p, acc, m, moved, captured, ep, toPiece, -1);

        p.undo();

        if(timeUp())return 0;

        if(s>best){
            best=s;
            bm=m;
            updatePV(0, m);
        }
        if(s>alpha)alpha=s;
        if(alpha>=beta)break;
    }

    const Bound rootBound = best <= alphaOrig ? Bound::Upper
                           : (best >= beta ? Bound::Lower : Bound::Exact);
    tt().store(rootKey,d,best,rootBound,0,bm);
    return best;
}

int Search::rootSearchParallel(Position&p,NNUE::Accumulator&acc,int d,int alpha,int beta,Move&bm){
    std::vector<Move>& mv = moveList_[0];
    p.generateLegal(mv);
    if(mv.empty()){
        bm={};
        return p.inCheck(p.side()==1)?-MATE:0;
    }

    std::sort(mv.begin(),mv.end(),[&](const Move&x,const Move&y){
        return moveScore(p,x,0,hot_.pvMove,Move{})>moveScore(p,y,0,hot_.pvMove,Move{});
    });

    if(hot_.threads<=1||mv.size()<=1)
        return rootSearch(p,acc,d,alpha,beta,bm);

    // v1.0.5: Young-Brothers-Wait style root coordination. Search the first
    // ordered root move synchronously to establish a strong alpha bound, then
    // let the coordinator + worker threads consume the remaining root queue.
    // Each worker owns its own TT/heuristics, so the search hot path has no TT
    // mutex. The shared atomic root alpha lets late workers avoid re-searching
    // obviously inferior root moves at a wide window.
    struct Result{Move m{};int score=-INF;bool done=false;};
    std::vector<Result> results(mv.size());
    std::atomic<size_t> next{1};
    std::atomic<int> rootAlpha{alpha};
    std::atomic<bool> cutoff{false};
    std::vector<std::thread> workers;

    bool firstCompleted=false;
    {
        const Move& rootMove=mv[0];
        int moved=p.at(rootMove.from);
        int captured=isCapture(rootMove)?p.at(rootMove.to):0;
        if(rootMove.flag==Flag::EnPassant)
            captured=p.at(rootMove.to+(moved>0?-8:8));
        Position child=p;
        if(child.make(rootMove)){
            NNUE::Accumulator ca=acc;
            network().applyMoveFeatures(child, ca, rootMove, moved, captured,
                                        rootMove.flag==Flag::EnPassant, child.at(rootMove.to), +1);
            int s=-negamax(child,ca,d-1,-beta,-alpha,1,rootMove);
            const bool completed=!timeUp();
            if(completed){
                results[0]={rootMove,s,true};
                rootAlpha.store(std::max(alpha,s),std::memory_order_release);
                firstCompleted=true;
                if(s>=beta) cutoff.store(true,std::memory_order_release);
            }
        }
    }

    if(timeUp() || cutoff.load(std::memory_order_acquire)){
        bm=firstCompleted?results[0].m:mv.front();
        if(firstCompleted) ++hot_.lazySmpCompleted;
        if(!firstCompleted) ++hot_.lazySmpAborted;
        return firstCompleted?results[0].score:hot_.score;
    }

    const auto deadline=hardDeadline_;
    const bool timed=hot_.timed;
    const int workerCount=std::min<int>(hot_.threads-1,static_cast<int>(mv.size()-1));
    hot_.lazySmpLaunches += static_cast<uint64_t>(workerCount);
    workers.reserve(workerCount);

    for(int t=0;t<workerCount;t++){
        Search* worker = parallelWorkers_[static_cast<size_t>(t)].get();
        worker->nodes_.store(0,std::memory_order_relaxed);
        workers.emplace_back([&,worker,deadline,timed](){
            worker->stop_.store(false,std::memory_order_relaxed);
            worker->externalStop_=&stop_;
            worker->hot_.timed=timed;
            worker->hardDeadline_=deadline;
            worker->softDeadline_=deadline;
            worker->hot_.useSoft=false;
            worker->hot_.pvMove=hot_.pvMove;

            for(;;){
                if(worker->timeUp() || cutoff.load(std::memory_order_acquire)) break;
                const size_t i=next.fetch_add(1,std::memory_order_relaxed);
                if(i>=mv.size()) break;

                const int localAlpha=std::max(alpha,rootAlpha.load(std::memory_order_acquire));
                if(localAlpha>=beta){
                    cutoff.store(true,std::memory_order_release);
                    break;
                }

                const Move& rootMove=mv[i];
                int moved=p.at(rootMove.from);
                int captured=isCapture(rootMove)?p.at(rootMove.to):0;
                if(rootMove.flag==Flag::EnPassant)
                    captured=p.at(rootMove.to+(moved>0?-8:8));

                Position child=p;
                bool completed=false;
                if(child.make(rootMove)){
                    NNUE::Accumulator ca=acc;
                    network().applyMoveFeatures(child, ca, rootMove, moved, captured,
                                                rootMove.flag==Flag::EnPassant, child.at(rootMove.to), +1);
                    int s=-worker->negamax(child,ca,d-1,-localAlpha-1,-localAlpha,1,rootMove);
                    if(s>localAlpha && s<beta && !worker->timeUp())
                        s=-worker->negamax(child,ca,d-1,-beta,-localAlpha,1,rootMove);
                    completed=!worker->timeUp();
                    if(completed){
                        results[i]={rootMove,s,true};
                        int cur=rootAlpha.load(std::memory_order_acquire);
                        while(s>cur && !rootAlpha.compare_exchange_weak(cur,s,
                               std::memory_order_acq_rel,std::memory_order_acquire)){}
                        if(s>=beta) cutoff.store(true,std::memory_order_release);
                    }
                }
                if(!completed && worker->timeUp()) break;
            }
        });
    }

    // Coordinator participates in the same queue, which is important on
    // 2-core and mid-range Android devices where one extra worker would be a
    // disproportionate scheduling cost.
    for(;;){
        if(timeUp() || cutoff.load(std::memory_order_acquire)) break;
        const size_t i=next.fetch_add(1,std::memory_order_relaxed);
        if(i>=mv.size()) break;

        const int localAlpha=std::max(alpha,rootAlpha.load(std::memory_order_acquire));
        if(localAlpha>=beta){
            cutoff.store(true,std::memory_order_release);
            break;
        }
        const Move& rootMove=mv[i];
        int moved=p.at(rootMove.from);
        int captured=isCapture(rootMove)?p.at(rootMove.to):0;
        if(rootMove.flag==Flag::EnPassant)
            captured=p.at(rootMove.to+(moved>0?-8:8));
        Position child=p;
        bool completed=false;
        if(child.make(rootMove)){
            NNUE::Accumulator ca=acc;
            network().applyMoveFeatures(child, ca, rootMove, moved, captured,
                                        rootMove.flag==Flag::EnPassant, child.at(rootMove.to), +1);
            int s=-negamax(child,ca,d-1,-localAlpha-1,-localAlpha,1,rootMove);
            if(s>localAlpha && s<beta && !timeUp())
                s=-negamax(child,ca,d-1,-beta,-localAlpha,1,rootMove);
            completed=!timeUp();
            if(completed){
                results[i]={rootMove,s,true};
                int cur=rootAlpha.load(std::memory_order_acquire);
                while(s>cur && !rootAlpha.compare_exchange_weak(cur,s,
                       std::memory_order_acq_rel,std::memory_order_acquire)){}
                if(s>=beta) cutoff.store(true,std::memory_order_release);
            }
        }
        if(!completed && timeUp()) break;
    }

    for(auto&th:workers) th.join();
    for(auto&worker:parallelWorkers_)
        nodes_.fetch_add(worker->nodes_.load(std::memory_order_relaxed),std::memory_order_relaxed);

    int best=-INF;
    Move bestM=mv.front();
    bool any=false;
    for(const auto&r:results){
        if(!r.done) continue;
        ++hot_.lazySmpCompleted;
        any=true;
        if(r.score>best){
            best=r.score;
            bestM=r.m;
            ++hot_.parallelBestUpdates;
        }
    }

    if(!any){
        ++hot_.lazySmpAborted;
        bm=firstCompleted?results[0].m:mv.front();
        return firstCompleted?results[0].score:hot_.score;
    }
    bm=bestM;
    return best;
}

Move Search::searchInternal(Position&p,int depth,int softMs,int hardMs){
    stop_.store(false, std::memory_order_relaxed);
    nodes_=0;
    hot_.score=0;
    hot_.completedDepth=0;
    hot_.rootBest={};
    hot_.pvMove={};
    hot_.pvLen[0] = 0;
    hot_.lastCompletedBest = {};
    hot_.lastCompletedScore = 0;
    hot_.lastDepthNodes = 0;
    hot_.estimatedNextDepthNodes = 0;
    hot_.aspirationSearches = 0;
    hot_.aspirationFailLows = 0;
    hot_.aspirationFailHighs = 0;
    hot_.pvsResearches = 0;
    hot_.lmrSearches = 0;
    hot_.lmrResearches = 0;
    hot_.nullMoveSearches = 0;
    hot_.nullMoveCutoffs = 0;
    hot_.nullMoveVerifications = 0;
    hot_.nullMoveVerificationFailures = 0;
    hot_.razorSearches = 0;
    hot_.razorFailLows = 0;
    hot_.lazySmpLaunches = 0;
    hot_.lazySmpCompleted = 0;
    hot_.lazySmpAborted = 0;
    hot_.parallelBestUpdates = 0;
    tt().newSearch();
    prepareParallelWorkers();

    searchStart_ = std::chrono::steady_clock::now();
    // Sanitize the two bounds here as a final safety net.  An untimed fixed-depth
    // search must remain truly untimed; timed searches always get a real hard
    // deadline and a soft deadline that never exceeds it.
    hot_.timed = (hardMs > 0);
    hot_.useSoft = false;
    hot_.budgetSoftMs = 0;
    hot_.budgetHardMs = 0;
    if (hot_.timed) {
        hardMs = std::max(1, hardMs);
        if (softMs <= 0) softMs = hardMs;
        softMs = std::clamp(softMs, 1, hardMs);
        hot_.useSoft = (softMs < hardMs);
        hot_.budgetSoftMs = softMs;
        hot_.budgetHardMs = hardMs;
        hardDeadline_ = searchStart_ + std::chrono::milliseconds(hardMs);
        softDeadline_ = searchStart_ + std::chrono::milliseconds(softMs);
    }

    // v0.87: preserve the configured thread count; rootSearchParallel()
    // activates Lazy SMP when Threads > 1.

    depth=std::clamp(depth,1,64);

    std::vector<Move>& legal = moveList_[0];
    p.generateLegal(legal);
    if(legal.empty())return{};

    hot_.rootBest=legal.front();

#ifdef CHESSZERO_FATHOM
    if (syzygy_) {
        SyzygyProbeResult tb;
        Move tbMove{};
        if (syzygy_->rootProbe(p, tbMove, tb)) {
            hot_.score=tbScore(tb.wdl);
            hot_.completedDepth=depth;
            hot_.rootBest=tbMove;
            hot_.timed=false;
            hot_.useSoft=false;
            return tbMove;
        }
    }
#endif

    if (p.isDraw()) {
        hot_.score = 0;
        hot_.completedDepth = 0;
        hot_.timed = false;
        hot_.useSoft = false;
        return hot_.rootBest;
    }

    // Warm move buffers once per search
    for (int i = 0; i < MAX_PLY; ++i)
        if (moveList_[i].capacity() < 64)
            moveList_[i].reserve(64);

    network().refresh(p, hot_.rootAcc);
    NNUE::Accumulator& rootAcc = hot_.rootAcc;

    int previous=0;

    // Iterative deepening with aspiration-window widening + hard/soft bounds.
    // The soft bound is deliberately checked only between completed depths: a
    // depth already in progress is allowed to finish cleanly unless the hard
    // bound fires inside the tree.
    for(int d=1;d<=depth;d++){
        if (d > 1 && softTimeUp())
            break;

        // v0.54: cost-aware iterative deepening.  Estimate the next depth
        // from the previous completed iteration and avoid entering a depth
        // that is very unlikely to finish before the soft target.  The hard
        // deadline still remains the absolute safety boundary.
        if (d > 1 && hot_.timed && hot_.useSoft && hot_.estimatedNextDepthNodes > 0) {
            const auto now = std::chrono::steady_clock::now();
            const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(softDeadline_ - now).count();
            if (remaining > 0) {
                const uint64_t prevNodes = std::max<uint64_t>(1, hot_.lastDepthNodes);
                const uint64_t projected = hot_.estimatedNextDepthNodes;
                const uint64_t safetyNodes = std::max<uint64_t>(2048, prevNodes / 8);
                // A node estimate is intentionally conservative: if the next
                // depth would likely consume the remaining budget plus a
                // small safety margin, keep the last completed iteration.
                const uint64_t roughNodesPerMs = std::max<uint64_t>(1, prevNodes / 10);
                const uint64_t remainingBudgetNodes = static_cast<uint64_t>(remaining) * roughNodesPerMs;
                if (projected + safetyNodes > remainingBudgetNodes)
                    break;
            }
        }

        // v0.77: configurable aspiration window with the proven v0.76
        // widening policy.  Keeping the default at 20cp preserves the
        // v0.76 search path while exposing tuning controls and diagnostics.
        int alpha=-INF,beta=INF;
        int window=(d>=4?std::clamp(hot_.aspirationWindow,4,128):INF);
        int failLowW=window, failHighW=window;

        if(hot_.aspiration && d>=4){
            alpha=std::max(-INF,previous-window);
            beta=std::min(INF,previous+window);
            ++hot_.aspirationSearches;
        }

        Move bm{};
        int s=0;

        while(true){
            s=(hot_.threads > 1 ? rootSearchParallel(p,rootAcc,d,alpha,beta,bm)
                                : rootSearch(p,rootAcc,d,alpha,beta,bm));

            if(timeUp())break;

            if(d<4 || !hot_.aspiration || (s>alpha && s<beta))
                break;

            if(s<=alpha){
                ++hot_.aspirationFailLows;
                // Preserve the original v0.76 fail-low expansion: double the
                // low-side window and anchor it on the fail-soft score.
                failLowW=std::min(INF,failLowW*2);
                alpha=std::max(-INF,s-failLowW);
            } else {
                ++hot_.aspirationFailHighs;
                failHighW=std::min(INF,failHighW*2);
                beta=std::min(INF,s+failHighW);
            }

            if(alpha<=-INF && beta>=INF){
                s=(hot_.threads > 1 ? rootSearchParallel(p,rootAcc,d,-INF,INF,bm)
                                    : rootSearch(p,rootAcc,d,-INF,INF,bm));
                break;
            }
        }

        if(timeUp())break;

        // Only accept this depth if we got a real result (not aborted empty)
        if (bm.from || bm.to || hot_.completedDepth == 0) {
            const uint64_t depthNodes = nodes_.load(std::memory_order_relaxed) - hot_.lastDepthNodes;
            previous=s;
            hot_.score=s;
            hot_.rootBest=bm;
            hot_.pvMove=bm;
            hot_.completedDepth=d;
            const Move priorBest = hot_.lastCompletedBest;
            const int priorScore = hot_.lastCompletedScore;
            hot_.lastCompletedBest=bm;
            hot_.lastCompletedScore=s;
            hot_.lastDepthNodes = nodes_.load(std::memory_order_relaxed);

            // v0.54: predict the cost of the next iteration.  Typical alpha-
            // beta growth is roughly exponential in depth, but TT/history
            // reuse makes the exact ratio noisy, so clamp it to a mobile-safe
            // range rather than trusting a single outlier.
            if (d >= 1) {
                const uint64_t base = std::max<uint64_t>(1, depthNodes);
                uint64_t ratio = (d <= 2) ? 10 : 5;
                if (hot_.lastDepthNodes > 0 && base > 0) {
                    const uint64_t prevNodes = std::max<uint64_t>(1, hot_.estimatedNextDepthNodes / std::max<uint64_t>(1, ratio));
                    if (prevNodes > 0) {
                        const uint64_t observed = std::clamp<uint64_t>(base * 100 / prevNodes, 120, 700);
                        ratio = std::clamp<uint64_t>((observed + 30) / 20, 3, 12);
                    }
                }
                hot_.estimatedNextDepthNodes = std::min<uint64_t>(base * ratio, UINT64_C(4000000000));
            }

            // If the PV/score is unstable, allow a small soft-budget extension
            // so a tactical swing is less likely to be cut immediately after a
            // completed depth. Never move the soft target past the hard bound.
            if (hot_.timed && hot_.useSoft && d > 1) {
                const bool moveChanged = !(bm == priorBest);
                const int scoreDelta = std::abs(s - priorScore);
                if (moveChanged || scoreDelta >= 80) {
                    const auto extension = std::chrono::milliseconds(
                        std::max<int64_t>(10, std::min<int64_t>(100, (softMs > 0 ? softMs / 5 : 50))));
                    const auto extended = softDeadline_ + extension;
                    if (extended < hardDeadline_) softDeadline_ = extended;
                    else softDeadline_ = hardDeadline_;
                }
            }
        }

        // Soft bound is a genuine move-time target.  Once the current depth
        // has completed beyond it, stop before starting another iteration.
        // This prevents a stable but unnecessarily long search from eating the
        // reserve intended to protect the clock.
        if (softTimeUp())
            break;
    }

    hot_.timed=false;
    hot_.useSoft=false;
    return hot_.rootBest;
}

Move Search::go(Position&p,int depth){
    return searchInternal(p,depth,0,0);
}

Move Search::go(Position&p,int depth,int movetimeMs){
    // For a fixed movetime, keep a 20% reserve for the GUI/OS and return path.
    const int hard = std::max(1, movetimeMs);
    const int soft = std::max(1, hard * 80 / 100);
    return searchInternal(p,depth,soft,hard);
}

Move Search::goTimed(Position&p,int maxDepth,int wtimeMs,int btimeMs,int wincMs,int bincMs,int movesToGo){
    // UCI clocks are milliseconds.  Negative values are invalid input; clamp
    // them rather than letting malformed GUI commands create a bad deadline.
    const int remain = std::max(0, p.side() == 1 ? wtimeMs : btimeMs);
    const int inc    = std::max(0, p.side() == 1 ? wincMs  : bincMs);

    // Without movestogo, reserve for a typical next ~30 moves.  With a sudden
    // death clock (movestogo=0), this is intentionally conservative.
    const int mtg = std::clamp(movesToGo > 0 ? movesToGo : 30, 1, 200);

    int soft = 1;
    int hard = 1;

    if (remain > 0) {
        // v0.98: subtract explicit GUI/OS/network overhead before budgeting the
        // move. This is deliberately separate from the percentage reserve so
        // users can tune it for a local GUI, Android, or remote tournament GUI.
        const int overhead = std::clamp(hot_.moveOverheadMs, 0, 1000);
        const int afterOverhead = std::max(1, remain - std::min(overhead, std::max(0, remain - 1)));

        // Keep an additional clock safety reserve. At most 250 ms, but never
        // less than 20 ms when enough time exists for such a reserve.
        const int reserve = std::clamp(afterOverhead / 20, 20, 250);
        const int usable = std::max(10, afterOverhead - reserve);

        // Spend the regular budget plus most of the increment. SlowMover=100 is
        // the baseline; higher values deliberately spend more of the usable
        // clock while the hard cap still prevents a flag.
        const int base = usable / mtg;
        const int incUse = std::min((inc * 3) / 4, usable / 6);
        const int rawSoft = base + incUse;
        soft = std::max(1, rawSoft * std::clamp(hot_.slowMover, 10, 1000) / 100);

        // Minimum thinking time prevents pathological 1-2 ms budgets, while
        // the usable-clock cap protects the low-clock case.
        const int softFloor = afterOverhead < 300 ? 5 : 10;
        const int softCap = std::max(softFloor, usable * 35 / 100);
        soft = std::clamp(soft, softFloor, softCap);

        // Hard = soft + controlled extension, capped well below the usable clock.
        const int extension = std::max(20, soft * 3 / 4);
        hard = std::min(soft + extension, usable * 60 / 100);
        hard = std::max(hard, soft);

        // Emergency mode: when the clock is already very low, spend a smaller
        // fraction and return a safe move quickly rather than risking a flag.
        if (remain < 500) {
            soft = std::max(5, usable * 30 / 100);
            hard = std::max(soft, usable * 55 / 100);
        }

        // Final absolute safety clamp. Never schedule the hard deadline at or
        // beyond the remaining clock minus explicit overhead.
        const int safeRemaining = std::max(1, remain - std::min(overhead, std::max(0, remain - 1)));
        hard = std::min(hard, std::max(1, safeRemaining - reserve));
        soft = std::min(soft, hard);
    }

    // The budget is exposed through telemetry so GUI/protocol tests can verify
    // that a requested clock never produces an unsafe allocation.
    hot_.budgetSoftMs = soft;
    hot_.budgetHardMs = hard;

    return searchInternal(p, std::clamp(maxDepth, 1, 64), soft, hard);
}

}
