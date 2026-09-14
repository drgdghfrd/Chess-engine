#include "Bitboard.h"
namespace uc { namespace BitOps {
static int rf(int sq){return sq>>3;} static int ff(int sq){return sq&7;}
Bitboard knight(int sq){Bitboard r=0;int r0=rf(sq),f0=ff(sq);static const int d[8][2]={{1,2},{2,1},{2,-1},{1,-2},{-1,-2},{-2,-1},{-2,1},{-1,2}};for(auto&a:d){int f=f0+a[0],rnk=r0+a[1];if(f>=0&&f<8&&rnk>=0&&rnk<8)r|=1ULL<<(rnk*8+f);}return r;}
Bitboard king(int sq){Bitboard r=0;int r0=rf(sq),f0=ff(sq);for(int dr=-1;dr<=1;dr++)for(int df=-1;df<=1;df++)if(dr||df){int f=f0+df,rnk=r0+dr;if(f>=0&&f<8&&rnk>=0&&rnk<8)r|=1ULL<<(rnk*8+f);}return r;}
Bitboard pawn(int sq,bool white){Bitboard r=0;int r0=rf(sq),f0=ff(sq),dr=white?1:-1;int nr=r0+dr;if(nr>=0&&nr<8){if(f0>0)r|=1ULL<<(nr*8+f0-1);if(f0<7)r|=1ULL<<(nr*8+f0+1);}return r;}
Bitboard rook(int sq,Bitboard occ){Bitboard r=0;int r0=rf(sq),f0=ff(sq);for(int d: {-1,1}){for(int f=f0+d;f>=0&&f<8;f+=d){Bitboard b=1ULL<<(r0*8+f);r|=b;if(occ&b)break;}}for(int d: {-1,1}){for(int rr=r0+d;rr>=0&&rr<8;rr+=d){Bitboard b=1ULL<<(rr*8+f0);r|=b;if(occ&b)break;}}return r;}
Bitboard bishop(int sq,Bitboard occ){Bitboard r=0;int r0=rf(sq),f0=ff(sq);static const int d[4][2]={{1,1},{1,-1},{-1,1},{-1,-1}};for(auto&a:d){int f=f0+a[0],rr=r0+a[1];while(f>=0&&f<8&&rr>=0&&rr<8){Bitboard b=1ULL<<(rr*8+f);r|=b;if(occ&b)break;f+=a[0];rr+=a[1];}}return r;}
Bitboard queen(int sq,Bitboard occ){return rook(sq,occ)|bishop(sq,occ);}
Bitboard attackersTo(int sq,const Bitboards& bb,bool byWhite){Bitboard occ=bb.white|bb.black,r=0;int idx=byWhite?0:6;r|=pawn(sq,!byWhite)&bb.piece[idx+0];r|=knight(sq)&bb.piece[idx+1];r|=king(sq)&bb.piece[idx+5];Bitboard diag=bishop(sq,occ);r|=diag&(bb.piece[idx+2]|bb.piece[idx+4]);Bitboard ortho=rook(sq,occ);r|=ortho&(bb.piece[idx+3]|bb.piece[idx+4]);return r;}
}}
