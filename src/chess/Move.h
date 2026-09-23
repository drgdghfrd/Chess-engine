#pragma once
#include <cstdint>
#include <string>
namespace uc {
enum class Flag : uint8_t { Quiet, Capture, DoublePawn, KingCastle, QueenCastle, EnPassant,
  PromoKnight, PromoBishop, PromoRook, PromoQueen,
  PromoKnightCapture, PromoBishopCapture, PromoRookCapture, PromoQueenCapture };
struct Move { uint8_t from=0,to=0; Flag flag=Flag::Quiet; bool operator==(const Move& o) const{return from==o.from&&to==o.to&&flag==o.flag;} };
bool isCapture(const Move& m); bool isPromotion(const Move& m); int promotionPiece(const Move& m); std::string toUci(const Move& m);
}
