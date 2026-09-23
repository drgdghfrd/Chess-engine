#include "../src/book/PolyglotZobrist.h"
#include <cassert>
#include <cstdint>
#include <iostream>
using namespace uc;
static void check(Position& p, const char* fen, uint64_t expected){
    assert(p.setFEN(fen));
    assert(PolyglotZobrist::key(p)==expected);
}
int main(){
    Position p;
    assert(PolyglotZobrist::isCanonicalTableReady());
    check(p,"rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1",0x463B96181691FC9Cull);
    check(p,"rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1",0x823C9B50FD114196ull);
    check(p,"rnbqkbnr/ppp1pppp/8/3p4/4P3/8/PPPP1PPP/RNBQKBNR w KQkq d6 0 2",0x0756B94461C50FB0ull);
    check(p,"rnbqkbnr/ppp1pppp/8/3pP3/8/8/PPPP1PPP/RNBQKBNR b KQkq - 0 2",0x662FAFB965DB29D4ull);
    check(p,"rnbqkbnr/ppp1p1pp/8/3pPp2/8/8/PPPP1PPP/RNBQKBNR w KQkq f6 0 3",0x22A48B5A8E47FF78ull);
    check(p,"rnbqkbnr/ppp1p1pp/8/3pPp2/8/8/PPPPKPPP/RNBQ1BNR b kq - 0 3",0x652A607CA3F242C1ull);
    std::cout << "v0.95 PolyGlot canonical vectors: PASS\n"
              << "table_entries=781 vectors=6\n";
}
