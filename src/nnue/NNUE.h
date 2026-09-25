#pragma once
#include "../chess/Position.h"
#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <cstddef>

namespace uc {

// HalfKP: feature = (ô vua của MÌNH, ô quân, loại+màu quân, bỏ vua ra khỏi
// danh sách "quân"). 64*64*10 = 40960 feature. Hidden 256, int8 weights.
class NNUE {
public:
    static constexpr int INPUTS = 40960;
    static constexpr int HIDDEN_SIZE = 256;  // per perspective; L1 total = 2*HIDDEN_SIZE
    static constexpr int L2_SIZE = 8;
    static constexpr int L3_SIZE = 32;
    enum class Arch { Single = 32, Multi = 64 };

    struct alignas(64) Accumulator {
        int16_t data[2][HIDDEN_SIZE];
        bool valid = false;
        // HalfKP: mỗi accumulator phải nhớ nó được tính theo vua ở đâu, vì
        // applyMoveFeatures() không có Position — cần biết ngay lúc vua đối
        // phương (không đổi) vẫn dùng đúng ô vua cũ.
        int whiteKingSq = -1;
        int blackKingSq = -1;

        int16_t* white() { return data[0]; }
        int16_t* black() { return data[1]; }
        const int16_t* white() const { return data[0]; }
        const int16_t* black() const { return data[1]; }

        void copyFeaturesFrom(const Accumulator& o) {
            std::memcpy(data, o.data, sizeof(data));
            valid = o.valid;
            whiteKingSq = o.whiteKingSq;
            blackKingSq = o.blackKingSq;
        }
    };

    NNUE() = default;

    bool loaded() const { return loaded_; }
    const std::string& file() const { return file_; }
    const char* architectureName() const noexcept;

    bool load(const char* path);
    bool loadMemory(const void* data, std::size_t size, const char* source = "memory");
    bool loadEmbedded();

    int evaluate(const Position& p) const;
    int evaluate(const Position& p, const Accumulator& acc) const;

    static int forwardPathForTest(const int16_t* a, const int8_t* w,
                                 int32_t bias, uint32_t shift, int pathId);

    void refresh(const Position& p, Accumulator& acc) const;
    void update(const Position& p, const Move& m, int moved,
                int captured, bool ep, Accumulator& acc) const;

    // ĐỔI CHỮ KÝ: thêm Position& — bắt buộc để quét lại bàn cờ khi vua đi.
    // Search.cpp cần sửa 7 chỗ gọi để truyền thêm p, không đổi gì khác.
    void applyMoveFeatures(const Position& p, Accumulator& acc, const Move& m,
                           int moved, int captured, bool ep, int toPiece,
                           int sign) const;

private:
    bool loaded_ = false;
    std::string file_;
    alignas(64) std::array<int16_t, HIDDEN_SIZE> bias_{};
    alignas(64) std::array<int8_t, INPUTS * HIDDEN_SIZE> weights_{};
    alignas(64) std::array<int8_t, HIDDEN_SIZE> outputWeights_{};
    int32_t outputBias_ = 0;
    uint32_t outputShift_ = 8;

    static int featureIndex(int ownKingSq, int piece, int sq, bool whitePerspective);
    void addFeature(Accumulator& acc, int piece, int sq, int sign) const;
    // Tính lại từ đầu CHỈ MỘT bên (dùng khi vua bên đó vừa đi).
    void refreshOneSide(const Position& p, Accumulator& acc, bool whitePerspective) const;
    int forward(const int16_t* a) const;
    // v1.0.9 multi-layer (HalfKAv2-style): L1 (both perspectives, clipped int8,
    // stm-first concat) -> L2 (clipped) -> L3 (clipped) -> scalar output.
    int forwardMulti(const int16_t* stm, const int16_t* other) const;
    Arch arch_ = Arch::Single;
    alignas(64) std::array<int8_t, 2 * HIDDEN_SIZE * L2_SIZE> l2Weights_{};
    alignas(64) std::array<int32_t, L2_SIZE> l2Bias_{};
    alignas(64) std::array<int8_t, L2_SIZE * L3_SIZE> l3Weights_{};
    alignas(64) std::array<int32_t, L3_SIZE> l3Bias_{};
};

const char* nnueSimdPath();

}  // namespace uc