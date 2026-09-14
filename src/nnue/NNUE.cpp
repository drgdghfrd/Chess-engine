#include "NNUE.h"
#include "nnue_weights.h"
#include <algorithm>
#include <cstring>
#include <fstream>

namespace uc {
namespace {
constexpr uint32_t MAGIC = 0x3445554E;  // "NUE4"
constexpr uint32_t VERSION = 3;
}  // namespace

NNUE::NNUE() {
    fallbackWeights();
    loaded_ = false;
}

bool NNUE::load_embedded_network() {
    constexpr std::size_t expected =
        8 + sizeof(w1_) + sizeof(b1_) + sizeof(w2_) + sizeof(b2_);

    if (NNUE_WEIGHTS_SIZE != expected || NNUE_WEIGHTS_SIZE < 8) {
        fallbackWeights();
        loaded_ = false;
        return false;
    }

    const std::uint8_t* data = NNUE_WEIGHTS;
    std::uint32_t magic = 0;
    std::uint32_t version = 0;
    std::memcpy(&magic, data, sizeof(magic));
    std::memcpy(&version, data + 4, sizeof(version));

    if (magic != MAGIC || version != VERSION) {
        fallbackWeights();
        loaded_ = false;
        return false;
    }

    std::size_t off = 8;
    std::memcpy(w1_.data(), data + off, sizeof(w1_)); off += sizeof(w1_);
    std::memcpy(b1_.data(), data + off, sizeof(b1_)); off += sizeof(b1_);
    std::memcpy(w2_.data(), data + off, sizeof(w2_)); off += sizeof(w2_);
    std::memcpy(&b2_, data + off, sizeof(b2_));

    loaded_ = true;
    return true;
}

int NNUE::featureIndex(int piece, int sq) {
    int type = std::abs(piece) - 1;
    int idx = piece > 0 ? type : type + 6;
    return idx * 64 + sq;
}

void NNUE::fallbackWeights() {
    w1_.fill(0);
    b1_.fill(0);
    w2_.fill(0);
    b2_ = 0;
    static const int values[6] = {100, 320, 330, 500, 900, 0};
    for (int h = 0; h < HIDDEN; ++h) {
        bool white = h < HIDDEN / 2;
        int type = h % 6;
        w2_[h] = static_cast<int16_t>(white ? values[type] : -values[type]);
        for (int sq = 0; sq < 64; ++sq) {
            int fi = (white ? type : type + 6) * 64 + sq;
            w1_[h * INPUTS + fi] = 16;
        }
    }
}

bool NNUE::load(const char* path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    uint32_t magic = 0, version = 0;
    f.read(reinterpret_cast<char*>(&magic), 4);
    f.read(reinterpret_cast<char*>(&version), 4);
    if (!f || magic != MAGIC || version != VERSION) return false;
    f.read(reinterpret_cast<char*>(w1_.data()), sizeof(w1_));
    f.read(reinterpret_cast<char*>(b1_.data()), sizeof(b1_));
    f.read(reinterpret_cast<char*>(w2_.data()), sizeof(w2_));
    f.read(reinterpret_cast<char*>(&b2_), sizeof(b2_));
    loaded_ = static_cast<bool>(f);
    if (!loaded_) fallbackWeights();
    return loaded_;
}

void NNUE::addFeature(Accumulator& a, int piece, int sq, int sign) const {
    if (piece == 0 || sq < 0 || sq >= 64) return;
    const int fi = featureIndex(piece, sq);
    for (int h = 0; h < HIDDEN; ++h)
        a.v[h] += sign * static_cast<int32_t>(w1_[h * INPUTS + fi]);
}

void NNUE::refresh(const Position& p, Accumulator& acc) const {
    for (int h = 0; h < HIDDEN; ++h) acc.v[h] = b1_[h];
    for (int sq = 0; sq < 64; ++sq) {
        int pc = p.at(sq);
        if (pc) addFeature(acc, pc, sq, +1);
    }
}

void NNUE::update(const Position& /*before*/, const Move& m, int movedPiece, int capturedPiece,
                  bool enPassant, Accumulator& acc) const {
    const bool white = movedPiece > 0;
    addFeature(acc, movedPiece, m.from, -1);
    if (capturedPiece && !enPassant) addFeature(acc, capturedPiece, m.to, -1);
    if (enPassant) {
        int capSq = m.to + (white ? -8 : 8);
        addFeature(acc, capturedPiece, capSq, -1);
    }
    int placed = movedPiece;
    if (isPromotion(m)) placed = (white ? 1 : -1) * promotionPiece(m);
    addFeature(acc, placed, m.to, +1);
    if (m.flag == Flag::KingCastle) {
        int rf = white ? 7 : 63, rt = white ? 5 : 61;
        addFeature(acc, white ? WR : BR, rf, -1);
        addFeature(acc, white ? WR : BR, rt, +1);
    } else if (m.flag == Flag::QueenCastle) {
        int rf = white ? 0 : 56, rt = white ? 3 : 59;
        addFeature(acc, white ? WR : BR, rf, -1);
        addFeature(acc, white ? WR : BR, rt, +1);
    }
}

int NNUE::evaluate(const Position& p, const Accumulator& acc) const {
    int64_t out = b2_;
    for (int h = 0; h < HIDDEN; ++h) {
        int32_t a = std::max<int32_t>(0, acc.v[h]);
        out += static_cast<int64_t>(a) * w2_[h];
    }
    int score = static_cast<int>(out / 16);
    return p.side() == 1 ? score : -score;
}

int NNUE::evaluate(const Position& p) const {
    Accumulator a;
    refresh(p, a);
    return evaluate(p, a);
}

}  // namespace uc
