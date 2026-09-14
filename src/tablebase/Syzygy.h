#pragma once
#include <string>
#include "../chess/Position.h"
namespace uc {
struct TablebaseStatus { bool configured=false; bool pathExists=false; bool eligible=false; bool probingAvailable=false; int pieces=0; std::string path; };
class Syzygy {
    std::string path_; int probeLimit_=5;
public:
    bool setPath(const std::string& path);
    void setProbeLimit(int n){probeLimit_=n<0?0:n;}
    const std::string& path() const {return path_;}
    int probeLimit() const {return probeLimit_;}
    TablebaseStatus status(const Position& p) const;
    // v0.9 deliberately does not fake tablebase answers. A real Syzygy probe
    // is enabled only when a probing backend is linked into a future build.
};
}
