#include "../src/book/PolyglotZobrist.h"
#include <cassert>
#include <iostream>
using namespace uc;
int main(){
    Position p; p.start();
    // Gate: never confuse ChessZero's private Zobrist key with PolyGlot.
    assert(PolyglotZobrist::isCanonicalTableReady());
    assert(PolyglotZobrist::key(p)==0x463B96181691FC9Cull);
    std::cout << "v0.94 PolyGlot canonical-hash gate: PASS\n"
              << "canonical_table_ready=true canonical_table_vendored=true\n";
}
