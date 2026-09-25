#include "../src/tablebase/Syzygy.h"
#include <cassert>
#include <iostream>

int main(){
    uc::Syzygy tb;
    uc::Position p;
    assert(p.setFEN("7k/8/8/8/8/8/6Q1/7K w - - 0 1"));
    auto st=tb.status(p);
#ifdef CHESSZERO_FATHOM
    assert(!st.probingAvailable || st.largest <= 7);
    std::cout << "v0.93 Syzygy real backend API: PASS\n";
#else
    assert(!st.probingAvailable);
    std::cout << "v0.93 Syzygy real backend: SKIP (build without Fathom)\n";
#endif
    return 0;
}
