#pragma once
#include "../chess/Position.h"
#include "../chess/Move.h"
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace uc {

enum class BookFormat { None, Text, Polyglot };

struct BookEntry {
    uint64_t key = 0;
    uint16_t rawMove = 0;
    uint16_t weight = 0;
    uint32_t learn = 0;
};

class OpeningBook {
    std::unordered_map<std::string, std::vector<std::string>> textEntries_;
    std::vector<BookEntry> polyglotEntries_;
    bool loaded_=false;
    BookFormat format_=BookFormat::None;
    static std::string key4(const std::string& fen);
    static uint16_t readBE16(const unsigned char* p);
    static uint32_t readBE32(const unsigned char* p);
    static uint64_t readBE64(const unsigned char* p);
    static Move decodePolyglotMove(const Position& p, uint16_t raw);
    Move probeText(const Position& p) const;
public:
    bool load(const std::string& path);
    bool loaded() const { return loaded_; }
    std::size_t size() const { return format_==BookFormat::Polyglot ? polyglotEntries_.size() : textEntries_.size(); }
    BookFormat format() const { return format_; }
    const char* formatName() const;
    Move probe(const Position& p) const;
    // Probe a standard PolyGlot book when the caller already has the
    // canonical 64-bit PolyGlot position key.
    Move probePolyglotKey(const Position& p, uint64_t key) const;
    Move probePolyglot(const Position& p) const;
};

} // namespace uc
