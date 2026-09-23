#include "Syzygy.h"
#include <filesystem>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <mutex>
#include <vector>

#ifdef CHESSZERO_FATHOM
#include <tbprobe.h>
#endif

namespace uc {

static unsigned pieceCount(const Position& p) {
    const auto& b = p.bitboards();
    return static_cast<unsigned>(BitOps::popcount(b.white | b.black));
}

struct PathScan { bool anyDirectory=false; bool hasWdl=false; bool hasDtz=false; };

static PathScan scanPathList(const std::string& pathList) {
    PathScan out;
#ifdef _WIN32
    constexpr char separator=';';
#else
    constexpr char separator=':';
#endif
    size_t start=0;
    while (start <= pathList.size()) {
        const size_t end=pathList.find(separator,start);
        const std::string token=pathList.substr(start,end==std::string::npos?std::string::npos:end-start);
        if (!token.empty()) {
            std::error_code ec;
            if (std::filesystem::is_directory(token,ec) && !ec) {
                out.anyDirectory=true;
                std::error_code ie;
                for (std::filesystem::recursive_directory_iterator it(token,ie),finish; it!=finish && !ie; it.increment(ie)) {
                    if (!it->is_regular_file()) continue;
                    const std::string ext=it->path().extension().string();
                    if (ext==".rtbw") out.hasWdl=true;
                    else if (ext==".rtbz") out.hasDtz=true;
                    if (out.hasWdl && out.hasDtz) break;
                }
            }
        }
        if (end==std::string::npos) break;
        start=end+1;
    }
    return out;
}

#ifdef CHESSZERO_FATHOM
namespace {

static SyzygyWDL convertWdl(unsigned w) {
    switch (w) {
        case TB_LOSS: return SyzygyWDL::Loss;
        case TB_BLESSED_LOSS: return SyzygyWDL::BlessedWin;
        case TB_DRAW: return SyzygyWDL::Draw;
        case TB_CURSED_WIN: return SyzygyWDL::CursedWin;
        case TB_WIN: return SyzygyWDL::Win;
        default: return SyzygyWDL::Unknown;
    }
}

static SyzygyDTZ dtzClass(unsigned w) {
    switch (w) {
        case TB_LOSS: return SyzygyDTZ::Loss;
        case TB_BLESSED_LOSS: return SyzygyDTZ::BlessedWin;
        case TB_DRAW: return SyzygyDTZ::Draw;
        case TB_CURSED_WIN: return SyzygyDTZ::CursedWin;
        case TB_WIN: return SyzygyDTZ::Win;
        default: return SyzygyDTZ::Unknown;
    }
}

static void bitboards(const Position& p,
                      uint64_t& white, uint64_t& black, uint64_t& kings,
                      uint64_t& queens, uint64_t& rooks, uint64_t& bishops,
                      uint64_t& knights, uint64_t& pawns) {
    const auto& b = p.bitboards();
    white = b.white;
    black = b.black;
    kings = b.piece[5] | b.piece[11];
    queens = b.piece[4] | b.piece[10];
    rooks = b.piece[3] | b.piece[9];
    bishops = b.piece[2] | b.piece[8];
    knights = b.piece[1] | b.piece[7];
    pawns = b.piece[0] | b.piece[6];
}

static int tbPromoteToFlag(unsigned promotes, bool capture) {
    switch (promotes) {
        case TB_PROMOTES_KNIGHT: return static_cast<int>(capture ? Flag::PromoKnightCapture : Flag::PromoKnight);
        case TB_PROMOTES_BISHOP: return static_cast<int>(capture ? Flag::PromoBishopCapture : Flag::PromoBishop);
        case TB_PROMOTES_ROOK: return static_cast<int>(capture ? Flag::PromoRookCapture : Flag::PromoRook);
        case TB_PROMOTES_QUEEN: return static_cast<int>(capture ? Flag::PromoQueenCapture : Flag::PromoQueen);
        default: return static_cast<int>(capture ? Flag::Capture : Flag::Quiet);
    }
}

static Move decodeMove(const Position& p, unsigned result) {
    int from = static_cast<int>(TB_GET_FROM(result));
    int to = static_cast<int>(TB_GET_TO(result));
    if (from < 0 || from >= 64 || to < 0 || to >= 64) return {};
    bool capture = p.at(to) != EMPTY;
    if (TB_GET_EP(result)) capture = true;
    const int flag = tbPromoteToFlag(TB_GET_PROMOTES(result), capture);
    return Move{static_cast<uint8_t>(from), static_cast<uint8_t>(to), static_cast<Flag>(flag)};
}

} // namespace
#endif

Syzygy::~Syzygy() {
#ifdef CHESSZERO_FATHOM
    if (initialized_) tb_free();
#endif
}

void Syzygy::setPath(const std::string& path) {
    path_ = path;
    pathExists_ = false;
    wdlFiles_ = false;
    dtzFiles_ = false;
    largest_ = 0;
#ifdef CHESSZERO_FATHOM
    if (initialized_) {
        tb_free();
        initialized_ = false;
    }
#endif
    if (path_.empty()) return;

    const PathScan scan = scanPathList(path_);
    pathExists_ = scan.anyDirectory;
    wdlFiles_ = scan.hasWdl;
    dtzFiles_ = scan.hasDtz;
#ifdef CHESSZERO_FATHOM
    if (pathExists_) initialized_=tb_init(path_.c_str());
    largest_=TB_LARGEST;
#endif
}

bool Syzygy::available(const Position& p) const {
    const unsigned pieces = pieceCount(p);
    return probeLimit_>0 && initialized_ && largest_>0 && pieces>=2 &&
           pieces<=static_cast<unsigned>(probeLimit_);
}

SyzygyStatus Syzygy::status(const Position& p) const {
    SyzygyStatus s;
    s.probeLimit=probeLimit_;
    s.configured=!path_.empty();
    s.pathExists=pathExists_;
    s.wdlFiles=wdlFiles_;
    s.dtzFiles=dtzFiles_;
    s.largest=largest_;
    s.pieces = static_cast<int>(pieceCount(p));
    s.eligible=probeLimit_>0&&s.pieces>=2&&s.pieces<=probeLimit_;
    s.probingAvailable=initialized_&&largest_>0&&s.eligible;
    return s;
}

SyzygyProbeResult Syzygy::probe(const Position& p) const {
    SyzygyProbeResult r;
#ifdef CHESSZERO_FATHOM
    if (!available(p) || p.castlingRights()!=0 || p.halfmoveClock()!=0) return r;
    uint64_t white,black,kings,queens,rooks,bishops,knights,pawns;
    bitboards(p,white,black,kings,queens,rooks,bishops,knights,pawns);
    const unsigned ep=p.epSquare()<0?0u:static_cast<unsigned>(p.epSquare());
    const unsigned turn=p.side()==1?1u:0u;
    const unsigned w=tb_probe_wdl(white,black,kings,queens,rooks,bishops,knights,pawns,0,0,ep,turn);
    if(w!=TB_RESULT_FAILED){ r.wdl=convertWdl(w); r.wdlAvailable=true; }
#else
    (void)p;
#endif
    return r;
}

bool Syzygy::rootProbe(const Position& p, Move& bestMove, SyzygyProbeResult& result) const {
    result = {};
#ifdef CHESSZERO_FATHOM
    if (!available(p)) return false;
    uint64_t white,black,kings,queens,rooks,bishops,knights,pawns;
    bitboards(p,white,black,kings,queens,rooks,bishops,knights,pawns);
    const unsigned ep=p.epSquare()<0?0u:static_cast<unsigned>(p.epSquare());
    const unsigned turn=p.side()==1?1u:0u;
    const unsigned rule50=static_cast<unsigned>(std::max(0,p.halfmoveClock()));
    const unsigned castling=static_cast<unsigned>(p.castlingRights());
    if (castling != 0) return false;

    const unsigned root=tb_probe_root(white,black,kings,queens,rooks,bishops,knights,pawns,
        rule50,0,ep,turn,nullptr);
    if(root!=TB_RESULT_FAILED && root!=TB_RESULT_STALEMATE && root!=TB_RESULT_CHECKMATE) {
        result.wdl=convertWdl(TB_GET_WDL(root));
        result.wdlAvailable=true;
        result.dtz=dtzClass(TB_GET_WDL(root));
        result.dtzAvailable=true;
        result.dtzPlies=static_cast<int>(TB_GET_DTZ(root));
        result.rootMove=decodeMove(p,root);
        result.rootMoveAvailable=!(result.rootMove.from==0 && result.rootMove.to==0 && result.rootMove.flag==Flag::Quiet);
        if(result.rootMoveAvailable){ bestMove=result.rootMove; return true; }
    }

    TbRootMoves rm{};
    if (tb_probe_root_wdl(white,black,kings,queens,rooks,bishops,knights,pawns,
                          rule50,0,ep,turn,true,&rm) == 0 || rm.size == 0)
        return false;

    unsigned bestIndex=0;
    for(unsigned i=1;i<rm.size;++i) {
        if(rm.moves[i].tbRank > rm.moves[bestIndex].tbRank ||
           (rm.moves[i].tbRank == rm.moves[bestIndex].tbRank &&
            rm.moves[i].tbScore > rm.moves[bestIndex].tbScore)) bestIndex=i;
    }

    const int rank=rm.moves[bestIndex].tbRank;
    if(rank>=1000) result.wdl=SyzygyWDL::Win;
    else if(rank>=899) result.wdl=SyzygyWDL::CursedWin;
    else if(rank<=-1000) result.wdl=SyzygyWDL::Loss;
    else if(rank<=-899) result.wdl=SyzygyWDL::BlessedWin;
    else result.wdl=SyzygyWDL::Draw;
    result.wdlAvailable=true;
    result.dtz=SyzygyDTZ::Unknown;
    result.dtzAvailable=false;

    const TbMove tbm=rm.moves[bestIndex].move;
    const int from=static_cast<int>(TB_MOVE_FROM(tbm));
    const int to=static_cast<int>(TB_MOVE_TO(tbm));
    if(from<0 || from>=64 || to<0 || to>=64) return false;
    const bool capture=p.at(to)!=EMPTY || p.epSquare()==to;
    const int flag=tbPromoteToFlag(TB_MOVE_PROMOTES(tbm),capture);
    result.rootMove=Move{static_cast<uint8_t>(from),static_cast<uint8_t>(to),static_cast<Flag>(flag)};
    result.rootMoveAvailable=!(result.rootMove.from==0 && result.rootMove.to==0 && result.rootMove.flag==Flag::Quiet);
    if(!result.rootMoveAvailable) return false;
    bestMove=result.rootMove;
    return true;
#else
    (void)p; (void)bestMove; (void)result;
    return false;
#endif
}

}  // namespace uc

#ifdef CHESSZERO_FATHOM
#include <tbprobe.c>
#endif
