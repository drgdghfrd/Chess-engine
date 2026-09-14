#pragma once
#include <array>
#include <cstdint>
#include "../chess/Position.h"
#include "../chess/Move.h"

namespace uc {

class NNUE {
public:
    static constexpr int INPUTS = 12 * 64;
    static constexpr int HIDDEN = 128;

    struct Accumulator {
        std::array<int32_t, HIDDEN> v{};
    };

    NNUE();

    // Load the compiled-in network. Returns true only when the embedded blob
    // matches the current NNUE binary format exactly.
    bool load_embedded_network();

    bool load(const char* path);
    bool loaded() const { return loaded_; }

    void refresh(const Position& p, Accumulator& acc) const;
    void update(const Position& before, const Move& m, int movedPiece, int capturedPiece,
                bool enPassant, Accumulator& acc) const;

    int evaluate(const Position& p) const;
    int evaluate(const Position& p, const Accumulator& acc) const;

private:
    bool loaded_ = false;
    std::array<int16_t, INPUTS * HIDDEN> w1_{};
    std::array<int16_t, HIDDEN> b1_{};
    std::array<int16_t, HIDDEN> w2_{};
    int32_t b2_ = 0;

    void fallbackWeights();
    static int featureIndex(int piece, int sq);
    void addFeature(Accumulator& a, int piece, int sq, int sign) const;
};

}  // namespace uc
