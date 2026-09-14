#pragma once
#include <cstdint>
#include <array>

namespace uc {
using Bitboard=uint64_t;
struct Bitboards;
namespace BitOps {
inline int popcount(Bitboard x){return __builtin_popcountll(x);}
inline int lsb(Bitboard x){return x?__builtin_ctzll(x):-1;}
inline Bitboard north(Bitboard x){return x<<8;}
inline Bitboard south(Bitboard x){return x>>8;}
inline Bitboard east(Bitboard x){return (x&0xfefefefefefefefeULL)<<1;}
inline Bitboard west(Bitboard x){return (x&0x7f7f7f7f7f7f7f7fULL)>>1;}
Bitboard knight(int sq);
Bitboard king(int sq);
Bitboard pawn(int sq,bool white);
Bitboard rook(int sq,Bitboard occ);
Bitboard bishop(int sq,Bitboard occ);
Bitboard queen(int sq,Bitboard occ);
Bitboard attackersTo(int sq,const Bitboards& bb,bool byWhite);
}
struct Bitboards{Bitboard white=0,black=0,piece[12]{};};
}
