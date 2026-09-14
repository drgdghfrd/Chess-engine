#pragma once
#include "Move.h"
#include <array>
#include <string>
#include <vector>
#include "../bitboard/Bitboard.h"
namespace uc {
enum Piece{EMPTY=0,WP=1,WN=2,WB=3,WR=4,WQ=5,WK=6,BP=-1,BN=-2,BB=-3,BR=-4,BQ=-5,BK=-6};
struct State{Move move;int captured=0,castling=0,ep=-1,half=0,full=1;};
class Position{
 std::array<int,64> b_{}; int side_=1,castling_=0,ep_=-1,half_=0,full_=1; std::vector<State> hist_;
 void pawns(std::vector<Move>&,int,bool)const; void knights(std::vector<Move>&,int,bool)const; void sliders(std::vector<Move>&,int,bool,const int[][2],int)const; void king(std::vector<Move>&,int,bool)const;
 public:
 Position(); void start(); bool setFEN(const std::string&); std::string fen()const; std::string boardString()const;
 int at(int sq)const{return b_[sq];} int side()const{return side_;} int castlingRights()const{return castling_;} int epSquare()const{return ep_;} int parseSq(const std::string&s)const; static std::string sqName(int);
 bool attacked(int,bool)const; bool inCheck(bool)const; std::vector<Move> pseudo()const; std::vector<Move> legal()const; bool make(const Move&); void undo(); void makeNull(); void undoNull(); bool over()const; Bitboards bitboards() const;
};
uint64_t perft(Position&,int);
}
