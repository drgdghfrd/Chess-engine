#include "Syzygy.h"
#include <filesystem>
namespace uc {
bool Syzygy::setPath(const std::string& path){path_=path;return std::filesystem::exists(path_);}
TablebaseStatus Syzygy::status(const Position& p) const {
    TablebaseStatus s{};s.configured=!path_.empty();s.pathExists=s.configured&&std::filesystem::exists(path_);s.path=path_;
    for(int i=0;i<64;i++) if(p.at(i)!=EMPTY) ++s.pieces;
    s.eligible=s.pieces<=probeLimit_;
    s.probingAvailable=false;
    return s;
}
}
