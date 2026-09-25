#pragma once
#include "../chess/Position.h"
#include "../chess/Move.h"
#include <string>
#include <vector>

namespace uc {

enum class SyzygyWDL { Loss=-2, CursedWin=-1, Draw=0, BlessedWin=1, Win=2, Unknown=3 };
enum class SyzygyDTZ { Loss=-2, CursedWin=-1, Draw=0, BlessedWin=1, Win=2, Unknown=3 };

struct SyzygyProbeResult {
    SyzygyWDL wdl=SyzygyWDL::Unknown;
    SyzygyDTZ dtz=SyzygyDTZ::Unknown;
    bool wdlAvailable=false;
    bool dtzAvailable=false;
    bool rootMoveAvailable=false;
    Move rootMove{};
    int dtzPlies=0;
};

struct SyzygyStatus {
    int pieces=0;
    int probeLimit=5;
    unsigned largest=0;
    bool eligible=false;
    bool configured=false;
    bool pathExists=false;
    bool wdlFiles=false;
    bool dtzFiles=false;
    bool probingAvailable=false;
};

class Syzygy {
    std::string path_;
    int probeLimit_=5;
    bool initialized_=false;
    bool pathExists_=false;
    bool wdlFiles_=false;
    bool dtzFiles_=false;
    unsigned largest_=0;
public:
    Syzygy() = default;
    ~Syzygy();
    void setPath(const std::string& path);
    void setProbeLimit(int n){probeLimit_=n<0?0:n>7?7:n;}
    const std::string& path() const{return path_;}
    int probeLimit() const{return probeLimit_;}
    SyzygyStatus status(const Position& p) const;
    SyzygyProbeResult probe(const Position& p) const;
    bool available(const Position& p) const;
    // Root DTZ/WDL probe. Returns true when Fathom produced a tablebase move.
    bool rootProbe(const Position& p, Move& bestMove, SyzygyProbeResult& result) const;
};

}  // namespace uc
