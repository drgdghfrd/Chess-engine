#pragma once
#include "../chess/Position.h"
#include "../chess/Move.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace uc {
class OpeningBook {
    std::unordered_map<std::string, std::vector<std::string>> entries_;
    bool loaded_=false;
    static std::string key4(const std::string& fen);
public:
    bool load(const std::string& path);
    bool loaded() const { return loaded_; }
    std::size_t size() const { return entries_.size(); }
    Move probe(const Position& p) const;
};
}
