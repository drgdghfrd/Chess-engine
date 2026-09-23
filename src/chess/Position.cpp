#include "Position.h"
#include "../bitboard/Bitboard.h"
#include <sstream>
#include <cctype>
#include <cstdlib>
#include <cmath>

namespace uc {

static bool own(int p, bool w) { return w ? p > 0 : p < 0; }
static bool opp(int p, bool w) { return w ? p < 0 : p > 0; }
static int ab(int p) { return p < 0 ? -p : p; }

Position::Position() {
    hist_.reserve(256);
    repKeys_.reserve(256);
    start();
}

void Position::addPieceBB(int pc, int sq) {
    if (pc == EMPTY || sq < 0 || sq >= 64) return;
    const int t = std::abs(pc) - 1;
    if (t < 0 || t > 5) return;
    const int i = pc > 0 ? t : t + 6;
    const Bitboard bit = 1ULL << sq;
    bb_.piece[i] |= bit;
    if (pc > 0) bb_.white |= bit; else bb_.black |= bit;
}

void Position::removePieceBB(int pc, int sq) {
    if (pc == EMPTY || sq < 0 || sq >= 64) return;
    const int t = std::abs(pc) - 1;
    if (t < 0 || t > 5) return;
    const int i = pc > 0 ? t : t + 6;
    const Bitboard bit = 1ULL << sq;
    bb_.piece[i] &= ~bit;
    if (pc > 0) bb_.white &= ~bit; else bb_.black &= ~bit;
}

void Position::refreshKingSq() {
    kingSq_[0] = kingSq_[1] = -1;
    for (int i = 0; i < 64; ++i) {
        if (b_[i] == WK) kingSq_[0] = i;
        else if (b_[i] == BK) kingSq_[1] = i;
    }
}

void Position::recomputeKey() {
    Zobrist::init();
    key_ = 0;
    for (int sq = 0; sq < 64; ++sq) {
        int pc = b_[sq];
        if (!pc) continue;
        int idx = zobristPieceIndex(pc);
        if (idx >= 0) key_ ^= Zobrist::piece[idx][sq];
    }
    if (side_ == -1) key_ ^= Zobrist::side;
    key_ ^= Zobrist::castling[castling_ & 15];
    if (ep_ >= 0) key_ ^= Zobrist::ep[ep_ & 7];
}

void Position::start() {
    BitOps::initAttackTables();
    setFEN("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1");
}

int Position::parseSq(const std::string& s) const {
    if (s.size() != 2) return -1;
    int f = s[0] - 'a', r = s[1] - '1';
    return (f < 0 || f > 7 || r < 0 || r > 7) ? -1 : r * 8 + f;
}

std::string Position::sqName(int s) {
    return std::string{char('a' + (s & 7)), char('1' + (s >> 3))};
}

bool Position::setFEN(const std::string& f) {
    std::istringstream ss(f);
    std::string pl, turn, cas, ep, h, fu;
    if (!(ss >> pl >> turn >> cas >> ep >> h >> fu)) return false;
    b_.fill(0);
    bb_ = Bitboards{};
    int sq = 56;
    for (char c : pl) {
        if (c == '/') { sq -= 16; continue; }
        if (std::isdigit(static_cast<unsigned char>(c))) { sq += c - '0'; continue; }
        int p = 0;
        switch (c) {
            case 'P': p = WP; break; case 'N': p = WN; break; case 'B': p = WB; break;
            case 'R': p = WR; break; case 'Q': p = WQ; break; case 'K': p = WK; break;
            case 'p': p = BP; break; case 'n': p = BN; break; case 'b': p = BB; break;
            case 'r': p = BR; break; case 'q': p = BQ; break; case 'k': p = BK; break;
            default: return false;
        }
        if (sq < 0 || sq >= 64) return false;
        b_[sq] = p;
        addPieceBB(p, sq);
        ++sq;
    }
    side_ = (turn == "w") ? 1 : -1;
    castling_ = 0;
    if (cas.find('K') != std::string::npos) castling_ |= 1;
    if (cas.find('Q') != std::string::npos) castling_ |= 2;
    if (cas.find('k') != std::string::npos) castling_ |= 4;
    if (cas.find('q') != std::string::npos) castling_ |= 8;
    ep_ = (ep == "-") ? -1 : parseSq(ep);
    if (ep != "-" && ep_ < 0) return false;
    half_ = std::stoi(h);
    full_ = std::stoi(fu);
    hist_.clear();
    repKeys_.clear();
    refreshKingSq();
    recomputeKey();
    repKeys_.push_back(key_);
    return true;
}

std::string Position::fen() const {
    std::ostringstream o;
    for (int r = 7; r >= 0; --r) {
        int e = 0;
        for (int f = 0; f < 8; ++f) {
            int p = b_[r * 8 + f];
            if (!p) { ++e; continue; }
            if (e) { o << e; e = 0; }
            char c = ".PNBRQK"[ab(p)];
            if (p < 0) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            o << c;
        }
        if (e) o << e;
        if (r) o << '/';
    }
    o << (side_ == 1 ? " w " : " b ");
    std::string c;
    if (castling_ & 1) c += 'K';
    if (castling_ & 2) c += 'Q';
    if (castling_ & 4) c += 'k';
    if (castling_ & 8) c += 'q';
    o << (c.empty() ? "-" : c) << ' ' << (ep_ < 0 ? "-" : sqName(ep_))
      << ' ' << half_ << ' ' << full_;
    return o.str();
}

bool Position::attacked(int s, bool byWhite) const {
    const auto& bb = bitboards();
    return BitOps::attackersTo(s, bb, byWhite) != 0;
}

bool Position::inCheck(bool white) const {
    int ksq = kingSq_[white ? 0 : 1];
    if (ksq < 0) return false;  // no king — not "in check"
    return attacked(ksq, !white);
}

void Position::pawns(std::vector<Move>& m, int s, bool w) const {
    int r = s >> 3, f = s & 7, dr = w ? 1 : -1, nr = r + dr, prom = w ? 7 : 0;
    if (nr < 0 || nr > 7) return;
    int t = nr * 8 + f;
    if (!b_[t]) {
        if (nr == prom) {
            m.push_back({(uint8_t)s, (uint8_t)t, Flag::PromoKnight});
            m.push_back({(uint8_t)s, (uint8_t)t, Flag::PromoBishop});
            m.push_back({(uint8_t)s, (uint8_t)t, Flag::PromoRook});
            m.push_back({(uint8_t)s, (uint8_t)t, Flag::PromoQueen});
        } else {
            m.push_back({(uint8_t)s, (uint8_t)t, Flag::Quiet});
            if ((w && r == 1) || (!w && r == 6)) {
                int t2 = (w ? r + 2 : r - 2) * 8 + f;
                if (!b_[t2])
                    m.push_back({(uint8_t)s, (uint8_t)t2, Flag::DoublePawn});
            }
        }
    }
    Bitboard caps = BitOps::pawn(s, w);
    while (caps) {
        int c = BitOps::lsb(caps);
        caps &= caps - 1;
        if (opp(b_[c], w)) {
            if ((c >> 3) == prom) {
                m.push_back({(uint8_t)s, (uint8_t)c, Flag::PromoKnightCapture});
                m.push_back({(uint8_t)s, (uint8_t)c, Flag::PromoBishopCapture});
                m.push_back({(uint8_t)s, (uint8_t)c, Flag::PromoRookCapture});
                m.push_back({(uint8_t)s, (uint8_t)c, Flag::PromoQueenCapture});
            } else {
                m.push_back({(uint8_t)s, (uint8_t)c, Flag::Capture});
            }
        } else if (c == ep_) {
            m.push_back({(uint8_t)s, (uint8_t)c, Flag::EnPassant});
        }
    }
}

void Position::knights(std::vector<Move>& m, int s, bool w) const {
    // ownMask from bitboards — caller should prefer batching; still O(1) table lookup
    const auto& bb = bitboards();
    Bitboard ownMask = w ? bb.white : bb.black;
    Bitboard a = BitOps::knight(s) & ~ownMask;
    while (a) {
        int t = BitOps::lsb(a);
        a &= a - 1;
        m.push_back({(uint8_t)s, (uint8_t)t, b_[t] ? Flag::Capture : Flag::Quiet});
    }
}

void Position::sliders(std::vector<Move>& m, int s, bool w) const {
    const auto& bb = bitboards();
    Bitboard a = 0;
    int p = ab(b_[s]);
    if (p == 3) a = BitOps::bishop(s, bb.white | bb.black);
    else if (p == 4) a = BitOps::rook(s, bb.white | bb.black);
    else a = BitOps::queen(s, bb.white | bb.black);
    Bitboard ownMask = w ? bb.white : bb.black;
    a &= ~ownMask;
    while (a) {
        int t = BitOps::lsb(a);
        a &= a - 1;
        m.push_back({(uint8_t)s, (uint8_t)t, b_[t] ? Flag::Capture : Flag::Quiet});
    }
}

void Position::king(std::vector<Move>& m, int s, bool w) const {
    Bitboard a = BitOps::king(s);
    Bitboard ownMask = w ? bitboards().white : bitboards().black;
    a &= ~ownMask;
    while (a) {
        int t = BitOps::lsb(a);
        a &= a - 1;
        m.push_back({(uint8_t)s, (uint8_t)t, b_[t] ? Flag::Capture : Flag::Quiet});
    }
    if (w && s == 4 && !inCheck(true)) {
        if ((castling_ & 1) && !b_[5] && !b_[6] && !attacked(5, false) && !attacked(6, false))
            m.push_back({4, 6, Flag::KingCastle});
        if ((castling_ & 2) && !b_[1] && !b_[2] && !b_[3] && !attacked(3, false) && !attacked(2, false))
            m.push_back({4, 2, Flag::QueenCastle});
    }
    if (!w && s == 60 && !inCheck(false)) {
        if ((castling_ & 4) && !b_[61] && !b_[62] && !attacked(61, true) && !attacked(62, true))
            m.push_back({60, 62, Flag::KingCastle});
        if ((castling_ & 8) && !b_[57] && !b_[58] && !b_[59] && !attacked(59, true) && !attacked(58, true))
            m.push_back({60, 58, Flag::QueenCastle});
    }
}

std::vector<Move> Position::pseudo() const {
    std::vector<Move> m;
    m.reserve(48);
    bool w = side_ == 1;
    // One occupancy snapshot for the whole generation
    const Bitboards& bb = bitboards();
    Bitboard ownMask = w ? bb.white : bb.black;
    Bitboard occ = bb.white | bb.black;

    for (int s = 0; s < 64; ++s) {
        int pc = b_[s];
        if (!own(pc, w)) continue;
        switch (ab(pc)) {
            case 1:
                pawns(m, s, w);
                break;
            case 2: {
                Bitboard a = BitOps::knight(s) & ~ownMask;
                while (a) {
                    int tsq = BitOps::lsb(a);
                    a &= a - 1;
                    m.push_back({(uint8_t)s, (uint8_t)tsq,
                                 b_[tsq] ? Flag::Capture : Flag::Quiet});
                }
                break;
            }
            case 3:
            case 4:
            case 5: {
                Bitboard a = 0;
                int pt = ab(pc);
                if (pt == 3) a = BitOps::bishop(s, occ);
                else if (pt == 4) a = BitOps::rook(s, occ);
                else a = BitOps::queen(s, occ);
                a &= ~ownMask;
                while (a) {
                    int tsq = BitOps::lsb(a);
                    a &= a - 1;
                    m.push_back({(uint8_t)s, (uint8_t)tsq,
                                 b_[tsq] ? Flag::Capture : Flag::Quiet});
                }
                break;
            }
            case 6:
                king(m, s, w);
                break;
        }
    }
    return m;
}

// OLD BUG: copied Position for EVERY move.
// NEW: one copy, then make/undo.
std::vector<Move> Position::legal() const {
    std::vector<Move> out;
    auto ms = pseudo();
    out.reserve(ms.size());
    Position q = *this;
    for (const auto& mv : ms) {
        if (q.make(mv)) {
            out.push_back(mv);
            q.undo();
        }
    }
    return out;
}

// Search path: no board copy at all.
void Position::generateLegal(std::vector<Move>& out) {
    // Single buffer: generate pseudo into `out`, then filter in-place.
    // Avoids allocating a second vector every node (cache + allocator friendly).
    out.clear();
    if (out.capacity() < 48) out.reserve(48);

    bool w = side_ == 1;
    const Bitboards& bb = bitboards();
    Bitboard ownMask = w ? bb.white : bb.black;
    Bitboard occ = bb.white | bb.black;

    for (int s = 0; s < 64; ++s) {
        int pc = b_[s];
        if (!own(pc, w)) continue;
        switch (ab(pc)) {
            case 1: pawns(out, s, w); break;
            case 2: {
                Bitboard a = BitOps::knight(s) & ~ownMask;
                while (a) {
                    int tsq = BitOps::lsb(a);
                    a &= a - 1;
                    out.push_back({(uint8_t)s, (uint8_t)tsq,
                                   b_[tsq] ? Flag::Capture : Flag::Quiet});
                }
                break;
            }
            case 3: case 4: case 5: {
                Bitboard a = 0;
                int pt = ab(pc);
                if (pt == 3) a = BitOps::bishop(s, occ);
                else if (pt == 4) a = BitOps::rook(s, occ);
                else a = BitOps::queen(s, occ);
                a &= ~ownMask;
                while (a) {
                    int tsq = BitOps::lsb(a);
                    a &= a - 1;
                    out.push_back({(uint8_t)s, (uint8_t)tsq,
                                   b_[tsq] ? Flag::Capture : Flag::Quiet});
                }
                break;
            }
            case 6: king(out, s, w); break;
        }
    }

    const size_t total = out.size();
    size_t widx = 0;
    for (size_t i = 0; i < total; ++i) {
        const Move mv = out[i];
        if (make(mv)) {
            undo();
            out[widx++] = mv;
        }
    }
    out.resize(widx);
}

bool Position::make(const Move& m) {
    // Public board-state guard: search normally calls make() only with pseudo-legal
    // moves, but UCI/debug callers can hand us an arbitrary Move value. Never let
    // an out-of-range, null, or own-piece destination mutate the position.
    if (m.from >= 64 || m.to >= 64 || m.from == m.to) return false;
    int moving = b_[m.from];
    bool w = side_ == 1;
    if (!own(moving, w)) return false;
    if (own(b_[m.to], w)) return false;
    // A legal chess move can never capture the opposing king. Reject it at
    // the board-state boundary so arbitrary debug/UCI callers cannot mutate
    // a position into a kingless state.
    if (ab(b_[m.to]) == 6) return false;

    Zobrist::init();
    State st{m, b_[m.to], castling_, ep_, half_, full_, key_, false, false};
    hist_.push_back(st);

    // --- Zobrist: remove moving piece from `from`
    int mi = zobristPieceIndex(moving);
    key_ ^= Zobrist::piece[mi][m.from];

    // Remove old castling / ep from hash (re-add after update)
    key_ ^= Zobrist::castling[castling_ & 15];
    if (ep_ >= 0) key_ ^= Zobrist::ep[ep_ & 7];

    b_[m.from] = 0;
    removePieceBB(moving, m.from);

    int captured = st.captured;
    if (m.flag == Flag::EnPassant) {
        int cs = m.to + (w ? -8 : 8);
        captured = b_[cs];
        hist_.back().captured = captured;
        int ci = zobristPieceIndex(captured);
        key_ ^= Zobrist::piece[ci][cs];
        b_[cs] = 0;
        removePieceBB(captured, cs);
    } else if (captured) {
        int ci = zobristPieceIndex(captured);
        key_ ^= Zobrist::piece[ci][m.to];
        removePieceBB(captured, m.to);
    }

    int placed = moving;
    if (isPromotion(m))
        placed = (w ? 1 : -1) * promotionPiece(m);
    b_[m.to] = placed;
    addPieceBB(placed, m.to);
    key_ ^= Zobrist::piece[zobristPieceIndex(placed)][m.to];

    // Castling rook move
    if (m.flag == Flag::KingCastle) {
        if (w) {
            key_ ^= Zobrist::piece[zobristPieceIndex(WR)][7];
            key_ ^= Zobrist::piece[zobristPieceIndex(WR)][5];
            b_[5] = WR; b_[7] = 0;
            removePieceBB(WR, 7); addPieceBB(WR, 5);
        } else {
            key_ ^= Zobrist::piece[zobristPieceIndex(BR)][63];
            key_ ^= Zobrist::piece[zobristPieceIndex(BR)][61];
            b_[61] = BR; b_[63] = 0;
            removePieceBB(BR, 63); addPieceBB(BR, 61);
        }
    } else if (m.flag == Flag::QueenCastle) {
        if (w) {
            key_ ^= Zobrist::piece[zobristPieceIndex(WR)][0];
            key_ ^= Zobrist::piece[zobristPieceIndex(WR)][3];
            b_[3] = WR; b_[0] = 0;
            removePieceBB(WR, 0); addPieceBB(WR, 3);
        } else {
            key_ ^= Zobrist::piece[zobristPieceIndex(BR)][56];
            key_ ^= Zobrist::piece[zobristPieceIndex(BR)][59];
            b_[59] = BR; b_[56] = 0;
            removePieceBB(BR, 56); addPieceBB(BR, 59);
        }
    }

    if (ab(moving) == 6) {
        kingSq_[w ? 0 : 1] = m.to;
        if (w) castling_ &= ~3;
        else   castling_ &= ~12;
    }

    if (m.from == 0 || m.to == 0) castling_ &= ~2;
    if (m.from == 7 || m.to == 7) castling_ &= ~1;
    if (m.from == 56 || m.to == 56) castling_ &= ~8;
    if (m.from == 63 || m.to == 63) castling_ &= ~4;

    ep_ = -1;
    if (ab(moving) == 1 && std::abs(int(m.to) - int(m.from)) == 16)
        ep_ = (m.from + m.to) / 2;

    // Re-add castling / ep
    key_ ^= Zobrist::castling[castling_ & 15];
    if (ep_ >= 0) key_ ^= Zobrist::ep[ep_ & 7];

    // Side to move flips every legal move
    key_ ^= Zobrist::side;

    if (ab(moving) == 1 || captured) half_ = 0;
    else ++half_;
    if (!w) ++full_;
    side_ = -side_;

    if (inCheck(w)) {
        undo();
        return false;
    }

    hist_.back().repAdded = true;
    repKeys_.push_back(key_);
    return true;
}

void Position::undo() {
    if (hist_.empty()) return;
    State st = hist_.back();
    hist_.pop_back();
    if (st.repAdded && repKeys_.size() > 1)
        repKeys_.pop_back();
    side_ = -side_;
    bool w = side_ == 1;
    auto& m = st.move;

    const int placed = b_[m.to];
    removePieceBB(placed, m.to);
    const int restored = isPromotion(m) ? (w ? WP : BP) : placed;
    b_[m.from] = restored;
    addPieceBB(restored, m.from);

    if (m.flag == Flag::EnPassant) {
        b_[m.to] = 0;
        int cs = m.to + (w ? -8 : 8);
        b_[cs] = st.captured;
        addPieceBB(st.captured, cs);
    } else {
        b_[m.to] = st.captured;
        if (st.captured) addPieceBB(st.captured, m.to);
    }

    if (m.flag == Flag::KingCastle) {
        if (w) { b_[7] = WR; b_[5] = 0; removePieceBB(WR, 5); addPieceBB(WR, 7); }
        else   { b_[63] = BR; b_[61] = 0; removePieceBB(BR, 61); addPieceBB(BR, 63); }
    } else if (m.flag == Flag::QueenCastle) {
        if (w) { b_[0] = WR; b_[3] = 0; removePieceBB(WR, 3); addPieceBB(WR, 0); }
        else   { b_[56] = BR; b_[59] = 0; removePieceBB(BR, 59); addPieceBB(BR, 56); }
    }

    if (ab(restored) == 6)
        kingSq_[w ? 0 : 1] = m.from;

    castling_ = st.castling;
    ep_ = st.ep;
    half_ = st.half;
    full_ = st.full;
    key_ = st.key;
}

void Position::makeNull() {
    Zobrist::init();
    State st{Move{}, 0, castling_, ep_, half_, full_, key_, false, true};
    hist_.push_back(st);
    if (ep_ >= 0) {
        key_ ^= Zobrist::ep[ep_ & 7];
        ep_ = -1;
    }
    key_ ^= Zobrist::side;
    ++half_;
    if (side_ == -1) ++full_;
    side_ = -side_;
}

void Position::undoNull() {
    if (hist_.empty()) return;
    State st = hist_.back();
    hist_.pop_back();
    castling_ = st.castling;
    ep_ = st.ep;
    half_ = st.half;
    full_ = st.full;
    side_ = -side_;
    key_ = st.key;
}


int Position::repetitionCount() const {
    if (repKeys_.empty()) return 0;

    const size_t cur = repKeys_.size() - 1;
    const size_t first = (half_ > static_cast<int>(cur))
        ? 0
        : cur - static_cast<size_t>(half_);

    int count = 0;
    for (size_t i = first; i <= cur; ++i)
        if (repKeys_[i] == key_)
            ++count;
    return count;
}

bool Position::isInsufficientMaterial() const {
    int knights = 0, bishops = 0, others = 0;
    int bishopColorMask = 0;

    for (int sq = 0; sq < 64; ++sq) {
        const int pc = b_[sq];
        switch (ab(pc)) {
            case 0:
            case 6:
                break;
            case 2:
                ++knights;
                break;
            case 3:
                ++bishops;
                bishopColorMask |= 1 << ((sq + (sq >> 3)) & 1);
                break;
            default:
                ++others;
                break;
        }
    }

    if (others != 0) return false;
    if (knights + bishops == 0) return true;
    if (knights + bishops == 1) return true;
    if (knights == 0 && bishops == 2 && bishopColorMask != 3) return true;
    return false;
}

const char* Position::drawReason() const {
    if (isDrawByRepetition()) return "repetition";
    if (isDrawBy50Move()) return "50move";
    if (isInsufficientMaterial()) return "insufficient";
    return "";
}

bool Position::over() const { return legal().empty(); }


std::string Position::boardString() const {
    std::ostringstream o;
    for (int r = 7; r >= 0; --r) {
        o << r + 1 << " ";
        for (int f = 0; f < 8; ++f) {
            int p = b_[r * 8 + f];
            o << (p == 0 ? '.' : "PNBRQKpnbrqk"[p > 0 ? p - 1 : 5 - p]) << ' ';
        }
        o << '\n';
    }
    o << "  a b c d e f g h\n";
    return o.str();
}

uint64_t perft(Position& p, int d) {
    if (d == 0) return 1;
    uint64_t n = 0;
    std::vector<Move> moves;
    p.generateLegal(moves);
    for (auto& m : moves) {
        p.make(m);
        n += perft(p, d - 1);
        p.undo();
    }
    return n;
}

}  // namespace uc
