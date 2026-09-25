#include "../src/chess/Position.h"
#include "../src/nnue/NNUE.h"
#include <cassert>
#include <fstream>
#include <iostream>

int main() {
    using namespace uc;
    Position p;
    NNUE n;
    NNUE::Accumulator a;

    assert(!n.loaded());
    assert(n.evaluate(p) == n.evaluate(p, a));

    n.refresh(p, a);
    assert(a.valid);
    for (auto x : a.white) assert(x == 0);
    for (auto x : a.black) assert(x == 0);

    // Malformed file must fail safely and leave fallback active.
    const char* bad = "v031_fake_nnue.bin";
    { std::ofstream f(bad, std::ios::binary); f << "not a net"; }
    assert(!n.load(bad));
    assert(!n.loaded());
    std::remove(bad);

    // Valid skeleton header loads without reading arbitrary weight payload.
    const char* good = "v031_valid_nnue.bin";
    struct H { char magic[8]; uint32_t version, inputs, hidden, reserved; } h{{'C','Z','N','N','U','E','3','1'},31,768,256,0};
    { std::ofstream f(good, std::ios::binary); f.write(reinterpret_cast<const char*>(&h), sizeof(h)); }
    assert(n.load(good));
    assert(n.loaded());
    assert(n.evaluate(p) == n.evaluate(p, a));
    std::remove(good);

    std::cout << "v0.31 NNUE skeleton tests: PASS\n";
    return 0;
}
