#include "Move.h"
namespace uc {
bool isCapture(const Move&m){auto f=m.flag;return f==Flag::Capture||f==Flag::EnPassant||f>=Flag::PromoKnightCapture;}
bool isPromotion(const Move&m){return m.flag>=Flag::PromoKnight;}
int promotionPiece(const Move&m){switch(m.flag){case Flag::PromoKnight:case Flag::PromoKnightCapture:return 2;case Flag::PromoBishop:case Flag::PromoBishopCapture:return 3;case Flag::PromoRook:case Flag::PromoRookCapture:return 4;case Flag::PromoQueen:case Flag::PromoQueenCapture:return 5;default:return 0;}}
std::string toUci(const Move&m){std::string s;s+=char('a'+(m.from&7));s+=char('1'+(m.from>>3));s+=char('a'+(m.to&7));s+=char('1'+(m.to>>3));if(isPromotion(m)){static const char p[]={'?','?','n','b','r','q','?','?'};s+=p[promotionPiece(m)];}return s;}
}
