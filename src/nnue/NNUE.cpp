#include "NNUE.h"
#include "../engine/Evaluation.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <vector>

#if defined(__unix__) || defined(__APPLE__) || defined(__ANDROID__)
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>
#define CZ_HAVE_MMAP 1
#endif

#if defined(__linux__)
#include <sys/auxv.h>
#endif

#if defined(USE_NEON) && (defined(__ARM_NEON) || defined(__ARM_NEON__))
#include <arm_neon.h>
#define CZ_NNUE_NEON 1
#endif

#if defined(CHESSZERO_EMBED_NNUE)
extern "C" {
extern const unsigned char _binary_embedded_nnue_bin_start[];
extern const unsigned char _binary_embedded_nnue_bin_end[];
}
#endif

namespace uc {

static bool cpuHasDotProd() {
#if defined(__aarch64__) && defined(__linux__)
#ifndef HWCAP_ASIMDDP
#define HWCAP_ASIMDDP (1UL << 20)
#endif
    static int cached = -1;
    if (cached < 0)
        cached = (getauxval(AT_HWCAP) & HWCAP_ASIMDDP) ? 1 : 0;
    return cached == 1;
#else
    return false;
#endif
}

static int classicEval(const Position& p) {
    return classicalEval(p);
}

struct NetHeader32 {
    char magic[8];
    uint32_t version;
    uint32_t inputs;
    uint32_t hidden;
    uint32_t output;
    uint32_t shift;
    uint32_t reserved;
};

static_assert(sizeof(NetHeader32) == 32, "unexpected NNUE header layout");

// HalfKP index. type 5 (vua) không bao giờ là "quân" — nó là điểm neo.
int NNUE::featureIndex(int ownKingSq, int piece, int sq, bool whitePerspective) {
    if (piece == EMPTY || sq < 0 || sq >= 64) return -1;
    const int type = std::abs(piece) - 1;      // 0=tot..5=vua
    if (type == 5) return -1;

    const bool own = whitePerspective ? piece > 0 : piece < 0;
    const int colorPlane = own ? 0 : 1;
    const int pieceIdx = colorPlane * 5 + type;        // 0..9

    const int kSq = whitePerspective ? ownKingSq : (ownKingSq ^ 56);
    const int pSq = whitePerspective ? sq        : (sq ^ 56);
    return (kSq * 64 + pSq) * 10 + pieceIdx;           // 0..40959
}

static inline void applyColumn(int16_t* __restrict acc,
                               const int8_t* __restrict col,
                               int sign) {
#if defined(CZ_NNUE_NEON)
    if (sign > 0) {
        for (int h = 0; h < NNUE::HIDDEN_SIZE; h += 32) {
            int16x8_t a0 = vld1q_s16(acc + h);
            int16x8_t a1 = vld1q_s16(acc + h + 8);
            int16x8_t a2 = vld1q_s16(acc + h + 16);
            int16x8_t a3 = vld1q_s16(acc + h + 24);
            int16x8_t w0 = vmovl_s8(vld1_s8(col + h));
            int16x8_t w1 = vmovl_s8(vld1_s8(col + h + 8));
            int16x8_t w2 = vmovl_s8(vld1_s8(col + h + 16));
            int16x8_t w3 = vmovl_s8(vld1_s8(col + h + 24));
            vst1q_s16(acc + h,      vaddq_s16(a0, w0));
            vst1q_s16(acc + h + 8,  vaddq_s16(a1, w1));
            vst1q_s16(acc + h + 16, vaddq_s16(a2, w2));
            vst1q_s16(acc + h + 24, vaddq_s16(a3, w3));
        }
    } else {
        for (int h = 0; h < NNUE::HIDDEN_SIZE; h += 32) {
            int16x8_t a0 = vld1q_s16(acc + h);
            int16x8_t a1 = vld1q_s16(acc + h + 8);
            int16x8_t a2 = vld1q_s16(acc + h + 16);
            int16x8_t a3 = vld1q_s16(acc + h + 24);
            int16x8_t w0 = vmovl_s8(vld1_s8(col + h));
            int16x8_t w1 = vmovl_s8(vld1_s8(col + h + 8));
            int16x8_t w2 = vmovl_s8(vld1_s8(col + h + 16));
            int16x8_t w3 = vmovl_s8(vld1_s8(col + h + 24));
            vst1q_s16(acc + h,      vsubq_s16(a0, w0));
            vst1q_s16(acc + h + 8,  vsubq_s16(a1, w1));
            vst1q_s16(acc + h + 16, vsubq_s16(a2, w2));
            vst1q_s16(acc + h + 24, vsubq_s16(a3, w3));
        }
    }
#else
    if (sign > 0) {
        for (int h = 0; h < NNUE::HIDDEN_SIZE; h += 8) {
            acc[h+0]=static_cast<int16_t>(acc[h+0]+col[h+0]);
            acc[h+1]=static_cast<int16_t>(acc[h+1]+col[h+1]);
            acc[h+2]=static_cast<int16_t>(acc[h+2]+col[h+2]);
            acc[h+3]=static_cast<int16_t>(acc[h+3]+col[h+3]);
            acc[h+4]=static_cast<int16_t>(acc[h+4]+col[h+4]);
            acc[h+5]=static_cast<int16_t>(acc[h+5]+col[h+5]);
            acc[h+6]=static_cast<int16_t>(acc[h+6]+col[h+6]);
            acc[h+7]=static_cast<int16_t>(acc[h+7]+col[h+7]);
        }
    } else {
        for (int h = 0; h < NNUE::HIDDEN_SIZE; h += 8) {
            acc[h+0]=static_cast<int16_t>(acc[h+0]-col[h+0]);
            acc[h+1]=static_cast<int16_t>(acc[h+1]-col[h+1]);
            acc[h+2]=static_cast<int16_t>(acc[h+2]-col[h+2]);
            acc[h+3]=static_cast<int16_t>(acc[h+3]-col[h+3]);
            acc[h+4]=static_cast<int16_t>(acc[h+4]-col[h+4]);
            acc[h+5]=static_cast<int16_t>(acc[h+5]-col[h+5]);
            acc[h+6]=static_cast<int16_t>(acc[h+6]-col[h+6]);
            acc[h+7]=static_cast<int16_t>(acc[h+7]-col[h+7]);
        }
    }
#endif
}

// Vẫn cộng/trừ cho CẢ HAI bên cùng lúc — dùng khi KHÔNG bên nào vừa đổi vua.
void NNUE::addFeature(Accumulator& acc, int piece, int sq, int sign) const {
    const int iw = featureIndex(acc.whiteKingSq, piece, sq, true);
    const int ib = featureIndex(acc.blackKingSq, piece, sq, false);
    if (iw >= 0) {
        const int8_t* col = weights_.data() + iw * HIDDEN_SIZE;
#if defined(__GNUC__)
        __builtin_prefetch(col, 0, 3);
        __builtin_prefetch(col + 64, 0, 3);
        __builtin_prefetch(col + 128, 0, 3);
        __builtin_prefetch(col + 192, 0, 3);
#endif
        applyColumn(acc.white(), col, sign);
    }
    if (ib >= 0) {
        const int8_t* col = weights_.data() + ib * HIDDEN_SIZE;
#if defined(__GNUC__)
        __builtin_prefetch(col, 0, 3);
        __builtin_prefetch(col + 64, 0, 3);
        __builtin_prefetch(col + 128, 0, 3);
        __builtin_prefetch(col + 192, 0, 3);
#endif
        applyColumn(acc.black(), col, sign);
    }
}

bool NNUE::loadMemory(const void* data, std::size_t size, const char* source) {
    if (!data || size < sizeof(NetHeader32)) return false;

    const auto* base = static_cast<const uint8_t*>(data);
    NetHeader32 h{};
    std::memcpy(&h, base, sizeof(h));
    if (std::strncmp(h.magic, "CZNNUE32", 8) != 0) return false;
    if (h.version != 32 || h.inputs != INPUTS || h.hidden != HIDDEN_SIZE || h.output != 1)
        return false;

    const size_t expectBias    = static_cast<size_t>(HIDDEN_SIZE) * sizeof(int16_t);
    const size_t expectWeights = static_cast<size_t>(INPUTS) * HIDDEN_SIZE * sizeof(int8_t);
    const size_t expectOutput  = static_cast<size_t>(HIDDEN_SIZE) * sizeof(int8_t);
    const size_t expectOBias   = sizeof(int32_t);
    const size_t totalExpected = sizeof(NetHeader32) + expectBias + expectWeights
                                + expectOutput + expectOBias;
    if (size < totalExpected) return false;

    size_t offset = sizeof(NetHeader32);
    std::memcpy(bias_.data(), base + offset, expectBias);
    offset += expectBias;
    std::memcpy(weights_.data(), base + offset, expectWeights);
    offset += expectWeights;
    std::memcpy(outputWeights_.data(), base + offset, expectOutput);
    offset += expectOutput;
    std::memcpy(&outputBias_, base + offset, expectOBias);

    outputShift_ = h.shift;
    loaded_ = true;
    file_ = source ? source : "memory";
    return true;
}

bool NNUE::load(const char* path) {
    if (!path || !*path) return false;

#if defined(CZ_HAVE_MMAP)
    // ---------------------------------------------------------------
    // POSIX mmap() path: zero-copy source access; parsed data is copied
    // into the fixed NNUE buffers and the mapping is released afterwards.
    // ---------------------------------------------------------------
    const int fd = ::open(path, O_RDONLY);
    if (fd < 0) return false;

    struct stat st{};
    if (::fstat(fd, &st) != 0) {
        ::close(fd);
        return false;
    }
    const size_t fileSize = static_cast<size_t>(st.st_size);
    if (fileSize == 0) {
        ::close(fd);
        return false;
    }

    void* const mapped = ::mmap(nullptr, fileSize, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mapped == MAP_FAILED) {
        ::close(fd);
        return false;
    }

    const bool ok = loadMemory(mapped, fileSize, path);
    ::munmap(mapped, fileSize);
    ::close(fd);
    return ok;

#else
    // ---------------------------------------------------------------
    // Portable fallback: read the ~10 MiB network once on the heap.
    // ---------------------------------------------------------------
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    in.seekg(0, std::ios::end);
    const std::streamoff end = in.tellg();
    if (end <= 0) return false;
    in.seekg(0, std::ios::beg);
    std::vector<uint8_t> bytes(static_cast<size_t>(end));
    in.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    if (!in) return false;
    return loadMemory(bytes.data(), bytes.size(), path);
#endif
}

bool NNUE::loadEmbedded() {
#if defined(CHESSZERO_EMBED_NNUE)
    const auto* begin = _binary_embedded_nnue_bin_start;
    const auto* end = _binary_embedded_nnue_bin_end;
    const std::size_t size = static_cast<std::size_t>(end - begin);
    return loadMemory(begin, size, "embedded:ChessZero-v0.75-halfkp.nnue");
#else
    return false;
#endif
}

void NNUE::refresh(const Position& p, Accumulator& acc) const {
    acc.whiteKingSq = p.kingSquare(true);
    acc.blackKingSq = p.kingSquare(false);
    std::memcpy(acc.white(), bias_.data(), HIDDEN_SIZE * sizeof(int16_t));
    std::memcpy(acc.black(), bias_.data(), HIDDEN_SIZE * sizeof(int16_t));
    acc.valid = true;
    if (!loaded_) return;
    const Bitboards& bb = p.bitboards();
    Bitboard occ = bb.white | bb.black;
    while (occ) {
        const int sq = BitOps::lsb(occ);
        occ &= occ - 1;
        addFeature(acc, p.at(sq), sq, +1);   // featureIndex tự bỏ qua vua
    }
}

// Tính lại CHỈ MỘT bên — dùng ngay sau khi vua bên đó đổi ô, để bên kia
// (vua không đổi) khỏi bị đụng tới.
void NNUE::refreshOneSide(const Position& p, Accumulator& acc, bool whitePerspective) const {
    int16_t* side = whitePerspective ? acc.white() : acc.black();
    std::memcpy(side, bias_.data(), HIDDEN_SIZE * sizeof(int16_t));
    if (!loaded_) return;
    const int ownKingSq = whitePerspective ? acc.whiteKingSq : acc.blackKingSq;
    const Bitboards& bb = p.bitboards();
    Bitboard occ = bb.white | bb.black;
    while (occ) {
        const int sq = BitOps::lsb(occ);
        occ &= occ - 1;
        const int idx = featureIndex(ownKingSq, p.at(sq), sq, whitePerspective);
        if (idx < 0) continue;
        applyColumn(side, weights_.data() + idx * HIDDEN_SIZE, +1);
    }
}

void NNUE::applyMoveFeatures(const Position& p, Accumulator& acc, const Move& m,
                             int moved, int captured, bool ep, int toPiece,
                             int sign) const {
    if (!loaded_ || !acc.valid) return;

    const bool isKing = std::abs(moved) == 6;
    if (isKing) {
        const bool whiteKingMoved = moved > 0;

        if (sign > 0) {
            // Forward path: p đã ở trạng thái SAU nước đi (đã make(m)).
            if (whiteKingMoved) acc.whiteKingSq = m.to;
            else                acc.blackKingSq = m.to;

            // Tính lại toàn bộ bên vừa đổi vua từ bàn cờ hiện tại.
            refreshOneSide(p, acc, whiteKingMoved);

            // Lưu lại bên vừa refresh — các addFeature phía dưới chỉ nên
            // ảnh hưởng đến BÊN KIA (đặc biệt là xe khi nhập thành).
            int16_t saved[HIDDEN_SIZE];
            int16_t* side = whiteKingMoved ? acc.white() : acc.black();
            std::memcpy(saved, side, sizeof(saved));

            // Vua không có feature (featureIndex loại type==5), nên các
            // addFeature cho moved/toPiece là no-op. Nhưng nếu là nhập
            // thành, phần xe ở dưới đây cập nhật bên kia cho sự di chuyển
            // của xe — đó là lý do chúng ta không return thẳng.
            addFeature(acc, moved, m.from, -sign);
            addFeature(acc, toPiece, m.to, +sign);
            if (captured) {
                int capturedSq = m.to;
                if (ep) capturedSq += moved > 0 ? -8 : 8;
                addFeature(acc, captured, capturedSq, -sign);
            }
            if (m.flag == Flag::KingCastle || m.flag == Flag::QueenCastle) {
                const bool white = moved > 0;
                const int rookFrom = white
                    ? (m.flag == Flag::KingCastle ? 7 : 0)
                    : (m.flag == Flag::KingCastle ? 63 : 56);
                const int rookTo = white
                    ? (m.flag == Flag::KingCastle ? 5 : 3)
                    : (m.flag == Flag::KingCastle ? 61 : 59);
                const int rook = white ? WR : BR;
                addFeature(acc, rook, rookFrom, -sign);
                addFeature(acc, rook, rookTo, +sign);
            }

            // Khôi phục lại bên vừa refreshOneSide.
            std::memcpy(side, saved, sizeof(saved));
        } else {
            // Backward path (undo): p vẫn ở trạng thái SAU nước đi (chưa
            // undo). Chúng ta cần đưa accumulator về trạng thái TRƯỚC.
            // Tạo bản sao của p, undo trên bản sao để có trạng thái trước.
            Position pCopy = p;
            pCopy.undo();  // bây giờ pCopy ở trạng thái trước nước đi

            if (whiteKingMoved) acc.whiteKingSq = m.from;
            else                acc.blackKingSq = m.from;

            // Tính lại bên có vua di chuyển từ trạng thái TRƯỚC.
            refreshOneSide(pCopy, acc, whiteKingMoved);

            // Lưu lại bên vừa refresh — các addFeature phía dưới hoàn tác
            // sự thay đổi của bên kia do xe nhập thành (hoặc no-op nếu
            // không phải nhập thành).
            int16_t saved[HIDDEN_SIZE];
            int16_t* side = whiteKingMoved ? acc.white() : acc.black();
            std::memcpy(saved, side, sizeof(saved));

            // Hoàn tác addFeature của forward path: đảo dấu sign.
            // Vua không có feature nên các dòng đầu là no-op. Phần xe
            // nhập thành (nếu có) sẽ hoàn tác sự di chuyển của xe đối với
            // bên kia.
            addFeature(acc, moved, m.from, -sign);  // sign=-1 → -sign=+1
            addFeature(acc, toPiece, m.to, +sign);  // sign=-1 → +sign=-1
            if (captured) {
                int capturedSq = m.to;
                if (ep) capturedSq += moved > 0 ? -8 : 8;
                addFeature(acc, captured, capturedSq, -sign);
            }
            if (m.flag == Flag::KingCastle || m.flag == Flag::QueenCastle) {
                const bool white = moved > 0;
                const int rookFrom = white
                    ? (m.flag == Flag::KingCastle ? 7 : 0)
                    : (m.flag == Flag::KingCastle ? 63 : 56);
                const int rookTo = white
                    ? (m.flag == Flag::KingCastle ? 5 : 3)
                    : (m.flag == Flag::KingCastle ? 61 : 59);
                const int rook = white ? WR : BR;
                addFeature(acc, rook, rookFrom, -sign);
                addFeature(acc, rook, rookTo, +sign);
            }

            // Khôi phục lại bên vừa refreshOneSide.
            std::memcpy(side, saved, sizeof(saved));
        }
        return;
    }

    addFeature(acc, moved, m.from, -sign);
    addFeature(acc, toPiece, m.to, +sign);
    if (captured) {
        int capturedSq = m.to;
        if (ep) capturedSq += moved > 0 ? -8 : 8;
        addFeature(acc, captured, capturedSq, -sign);
    }
    if (m.flag == Flag::KingCastle || m.flag == Flag::QueenCastle) {
        const bool white = moved > 0;
        const int rookFrom = white
            ? (m.flag == Flag::KingCastle ? 7 : 0)
            : (m.flag == Flag::KingCastle ? 63 : 56);
        const int rookTo = white
            ? (m.flag == Flag::KingCastle ? 5 : 3)
            : (m.flag == Flag::KingCastle ? 61 : 59);
        const int rook = white ? WR : BR;
        addFeature(acc, rook, rookFrom, -sign);
        addFeature(acc, rook, rookTo, +sign);
    }
}

void NNUE::update(const Position& p, const Move& m, int moved,
                  int captured, bool ep, Accumulator& acc) const {
    if (!loaded_ || !acc.valid) {
        refresh(p, acc);
        return;
    }
    applyMoveFeatures(p, acc, m, moved, captured, ep, p.at(m.to), +1);
}

static int __attribute__((unused)) forward_scalar(const int16_t* a, const int8_t* w, int32_t bias, uint32_t shift) {
    int32_t sum = bias;
    for (int h = 0; h < NNUE::HIDDEN_SIZE; h += 4) {
        int x0 = a[h];   if (x0 < 0) x0 = 0; else if (x0 > 127) x0 = 127;
        int x1 = a[h+1]; if (x1 < 0) x1 = 0; else if (x1 > 127) x1 = 127;
        int x2 = a[h+2]; if (x2 < 0) x2 = 0; else if (x2 > 127) x2 = 127;
        int x3 = a[h+3]; if (x3 < 0) x3 = 0; else if (x3 > 127) x3 = 127;
        sum += x0 * w[h] + x1 * w[h+1] + x2 * w[h+2] + x3 * w[h+3];
    }
    if (shift) sum >>= shift;
    if (sum > 30000) return 30000;
    if (sum < -30000) return -30000;
    return static_cast<int>(sum);
}

#if defined(CZ_NNUE_NEON)
static int forward_neon_mlal(const int16_t* a, const int8_t* w, int32_t bias, uint32_t shift) {
    int32x4_t s0 = vdupq_n_s32(0);
    int32x4_t s1 = vdupq_n_s32(0);
    int32x4_t s2 = vdupq_n_s32(0);
    int32x4_t s3 = vdupq_n_s32(0);
    const int16x8_t zero = vdupq_n_s16(0);
    const int16x8_t maxv = vdupq_n_s16(127);
    for (int h = 0; h < NNUE::HIDDEN_SIZE; h += 16) {
        int16x8_t x0 = vld1q_s16(a + h);
        int16x8_t x1 = vld1q_s16(a + h + 8);
        x0 = vminq_s16(vmaxq_s16(x0, zero), maxv);
        x1 = vminq_s16(vmaxq_s16(x1, zero), maxv);
        int16x8_t wv0 = vmovl_s8(vld1_s8(w + h));
        int16x8_t wv1 = vmovl_s8(vld1_s8(w + h + 8));
        s0 = vmlal_s16(s0, vget_low_s16(x0),  vget_low_s16(wv0));
        s1 = vmlal_s16(s1, vget_high_s16(x0), vget_high_s16(wv0));
        s2 = vmlal_s16(s2, vget_low_s16(x1),  vget_low_s16(wv1));
        s3 = vmlal_s16(s3, vget_high_s16(x1), vget_high_s16(wv1));
    }
    s0 = vaddq_s32(s0, s1);
    s2 = vaddq_s32(s2, s3);
    s0 = vaddq_s32(s0, s2);
    int32x2_t t2 = vadd_s32(vget_low_s32(s0), vget_high_s32(s0));
    int32_t sum = vget_lane_s32(t2, 0) + vget_lane_s32(t2, 1) + bias;
    if (shift) sum >>= shift;
    if (sum > 30000) return 30000;
    if (sum < -30000) return -30000;
    return static_cast<int>(sum);
}

#if defined(__aarch64__)
#if defined(__GNUC__)
__attribute__((target("+dotprod")))
#endif
static int forward_sdot(const int16_t* a, const int8_t* w, int32_t bias, uint32_t shift) {
    const int16x8_t zero = vdupq_n_s16(0);
    const int16x8_t maxv = vdupq_n_s16(127);
    int32x4_t acc0 = vdupq_n_s32(0);
    int32x4_t acc1 = vdupq_n_s32(0);
    for (int h = 0; h < NNUE::HIDDEN_SIZE; h += 32) {
        int16x8_t x0 = vld1q_s16(a + h);
        int16x8_t x1 = vld1q_s16(a + h + 8);
        x0 = vminq_s16(vmaxq_s16(x0, zero), maxv);
        x1 = vminq_s16(vmaxq_s16(x1, zero), maxv);
        int8x16_t xv0 = vcombine_s8(vqmovn_s16(x0), vqmovn_s16(x1));
        int8x16_t wv0 = vld1q_s8(w + h);
        acc0 = vdotq_s32(acc0, wv0, xv0);
        int16x8_t x2 = vld1q_s16(a + h + 16);
        int16x8_t x3 = vld1q_s16(a + h + 24);
        x2 = vminq_s16(vmaxq_s16(x2, zero), maxv);
        x3 = vminq_s16(vmaxq_s16(x3, zero), maxv);
        int8x16_t xv1 = vcombine_s8(vqmovn_s16(x2), vqmovn_s16(x3));
        int8x16_t wv1 = vld1q_s8(w + h + 16);
        acc1 = vdotq_s32(acc1, wv1, xv1);
    }
    acc0 = vaddq_s32(acc0, acc1);
    int32x2_t s2 = vadd_s32(vget_low_s32(acc0), vget_high_s32(acc0));
    int32_t sum = vget_lane_s32(s2, 0) + vget_lane_s32(s2, 1) + bias;
    if (shift) sum >>= shift;
    if (sum > 30000) return 30000;
    if (sum < -30000) return -30000;
    return static_cast<int>(sum);
}
#endif
#endif

int NNUE::forwardPathForTest(const int16_t* a, const int8_t* w,
                             int32_t bias, uint32_t shift, int pathId) {
#if defined(CZ_NNUE_NEON) && defined(__aarch64__)
    if (pathId == 1) return forward_sdot(a, w, bias, shift);
    return forward_neon_mlal(a, w, bias, shift);
#elif defined(CZ_NNUE_NEON)
    (void)pathId;
    return forward_neon_mlal(a, w, bias, shift);
#else
    (void)pathId;
    return forward_scalar(a, w, bias, shift);
#endif
}

int NNUE::forward(const int16_t* ap) const {
#if defined(__GNUC__)
    __builtin_prefetch(outputWeights_.data(), 0, 3);
    __builtin_prefetch(outputWeights_.data() + 64, 0, 3);
    __builtin_prefetch(outputWeights_.data() + 128, 0, 3);
    __builtin_prefetch(outputWeights_.data() + 192, 0, 3);
#endif
    if (!loaded_ || !ap) return 0;
    const int8_t* wp = outputWeights_.data();
#if defined(CZ_NNUE_NEON) && defined(__aarch64__)
    if (cpuHasDotProd())
        return forward_sdot(ap, wp, outputBias_, outputShift_);
    return forward_neon_mlal(ap, wp, outputBias_, outputShift_);
#elif defined(CZ_NNUE_NEON)
    return forward_neon_mlal(ap, wp, outputBias_, outputShift_);
#else
    return forward_scalar(ap, wp, outputBias_, outputShift_);
#endif
}

int NNUE::evaluate(const Position& p) const {
    if (!loaded_) return classicEval(p);
    Accumulator acc;
    refresh(p, acc);
    return evaluate(p, acc);
}

int NNUE::evaluate(const Position& p, const Accumulator& acc) const {
    if (!loaded_ || !acc.valid) return classicEval(p);
    return p.side() == 1 ? forward(acc.white()) : forward(acc.black());
}

const char* nnueSimdPath() {
#if defined(CZ_NNUE_NEON) && defined(__aarch64__)
    return cpuHasDotProd() ? "sdot" : "neon";
#elif defined(CZ_NNUE_NEON)
    return "neon";
#else
    return "scalar";
#endif
}

}  // namespace uc