#pragma once
#include "Move.h"
#include "Zobrist.h"
#include <array>
#include <cstdint>
#include <string>
#include <vector>
#include "../bitboard/Bitboard.h"

namespace uc {

enum Piece {
    EMPTY = 0,
    WP = 1, WN = 2, WB = 3, WR = 4, WQ = 5, WK = 6,
    BP = -1, BN = -2, BB = -3, BR = -4, BQ = -5, BK = -6
};

struct State {
    Move move;
    int captured = 0;
    int castling = 0;
    int ep = -1;
    int half = 0;
    int full = 1;
    uint64_t key = 0;  // hash before this move (for undo)
    bool repAdded = false;  // real move appended to repetition history
    bool isNull = false;    // null move never participates in repetition history
};

class Position {
    std::array<int, 64> b_{};
    Bitboards bb_{};
    int side_ = 1;
    int castling_ = 0;
    int ep_ = -1;
    int half_ = 0;
    int full_ = 1;
    int kingSq_[2] = {4, 60};
    uint64_t key_ = 0;
    std::vector<State> hist_;
    std::vector<uint64_t> repKeys_;  // keys of real positions on the current game/search path

    void pawns(std::vector<Move>&, int, bool) const;
    void knights(std::vector<Move>&, int, bool) const;
    void sliders(std::vector<Move>&, int, bool) const;
    void king(std::vector<Move>&, int, bool) const;
    void refreshKingSq();
    void recomputeKey();
    void addPieceBB(int pc, int sq);
    void removePieceBB(int pc, int sq);

public:
    Position();
    void start();
    bool setFEN(const std::string&);
    std::string fen() const;
    std::string boardString() const;

    int at(int sq) const { return b_[sq]; }
    int side() const { return side_; }
    int castlingRights() const { return castling_; }
    int epSquare() const { return ep_; }
    int kingSquare(bool white) const { return kingSq_[white ? 0 : 1]; }
    uint64_t key() const { return key_; }
    int halfmoveClock() const { return half_; }
    int repetitionCount() const;
    bool isDrawByRepetition() const { return repetitionCount() >= 3; }
    bool isDrawBy50Move() const { return half_ >= 100; }
    bool isInsufficientMaterial() const;
    bool isDraw() const { return isDrawByRepetition() || isDrawBy50Move() || isInsufficientMaterial(); }
    const char* drawReason() const;

    int parseSq(const std::string& s) const;
    static std::string sqName(int);

    bool attacked(int sq, bool byWhite) const;
    bool inCheck(bool white) const;

    std::vector<Move> pseudo() const;
    std::vector<Move> legal() const;
    void generateLegal(std::vector<Move>& out);

    bool make(const Move&);
    void undo();
    void makeNull();
    void undoNull();
    bool over() const;
    const Bitboards& bitboards() const { return bb_; }
};

uint64_t perft(Position&, int);

}  // namespace uc
