#include "../src/chess/Position.h"
#include "../src/nnue/NNUE.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using namespace uc;

static Move findMove(Position& p, const char* uci) {
    for (const auto& m : p.legal()) if (toUci(m) == uci) return m;
    return Move{};
}

static void writeNet(const char* path) {
    struct H { char magic[8]; uint32_t version, inputs, hidden, output, shift, reserved; };
    H h{{'C','Z','N','N','U','E','3','2'},32,NNUE::INPUTS,NNUE::HIDDEN_SIZE,1,8,0};
    std::ofstream f(path, std::ios::binary);
    f.write(reinterpret_cast<const char*>(&h), sizeof(h));
    for (int i=0;i<256;i++) { int16_t b = static_cast<int16_t>((i%9)-4); f.write(reinterpret_cast<char*>(&b),2); }
    for (int i=0;i<NNUE::INPUTS*NNUE::HIDDEN_SIZE;i++) { int8_t w = static_cast<int8_t>((i%7)-3); f.write(reinterpret_cast<char*>(&w),1); }
    for (int i=0;i<256;i++) { int8_t w = static_cast<int8_t>((i%11)-5); f.write(reinterpret_cast<char*>(&w),1); }
    int32_t ob=17; f.write(reinterpret_cast<char*>(&ob),4);
}

static void assertAccEqual(const NNUE::Accumulator& a, const NNUE::Accumulator& b) {
    assert(a.valid && b.valid);
    for(int i=0; i<256; ++i) assert(a.white()[i] == b.white()[i]);
    for(int i=0; i<256; ++i) assert(a.black()[i] == b.black()[i]);
}

int main() {
    // NNUE owns the ~10 MiB HalfKP weight matrix; keep large instances off
    // the thread stack. This matters on Termux/Android where stack limits can
    // be much smaller than desktop Linux.
    auto n = std::make_unique<NNUE>();
    Position p;
    auto fallback = std::make_unique<NNUE>();
    const int classic = fallback->evaluate(p);
    assert(!n->loaded());

    const char* bad = "v032_bad.nnue";
    { std::ofstream f(bad, std::ios::binary); f << "bad"; }
    assert(!n->load(bad));
    std::remove(bad);

    const char* good = "v032_test.nnue";
    writeNet(good);
    assert(n->load(good));
    assert(n->loaded());

    // Real inference must differ from the classic fallback on this position.
    NNUE::Accumulator a, b;
    n->refresh(p, a);
    assert(a.valid);
    const int nn = n->evaluate(p, a);
    assert(nn >= -30000 && nn <= 30000);
    assert(nn != classic);

    // Incremental updates must be bit-exact with a fresh refresh.
    const char* seq[] = {"e2e4","e7e5","g1f3","b8c6","f1b5","a7a6","b5a4","g8f6"};
    for (const char* u : seq) {
        Move m=findMove(p,u);
        assert(m.from != m.to || m.from != 0 || m.flag != Flag::Quiet);
        int moved=p.at(m.from);
        int captured=isCapture(m)?p.at(m.to):0;
        if(m.flag==Flag::EnPassant) captured=p.at(m.to+(moved>0?-8:8));
        assert(p.make(m));
        b = a;
        n->update(p,m,moved,captured,m.flag==Flag::EnPassant,b);
        NNUE::Accumulator fresh;
        n->refresh(p,fresh);
        assertAccEqual(b,fresh);
        assert(n->evaluate(p,b)==n->evaluate(p,fresh));
        a=b;
    }

    // Castling delta.
    Position c;
    assert(c.setFEN("r3k2r/8/8/8/8/8/8/R3K2R w KQkq - 0 1"));
    NNUE::Accumulator ca, cb, cf;
    n->refresh(c,ca);
    Move cm=findMove(c,"e1g1");
    int moved=c.at(cm.from), captured=isCapture(cm)?c.at(cm.to):0;
    assert(c.make(cm));
    cb=ca; n->update(c,cm,moved,captured,false,cb); n->refresh(c,cf); assertAccEqual(cb,cf);

    // En-passant delta.
    Position ep;
    assert(ep.setFEN("4k3/8/8/3pP3/8/8/8/4K3 w - d6 0 1"));
    NNUE::Accumulator ea, eb, ef;
    n->refresh(ep,ea);
    Move em=findMove(ep,"e5d6");
    moved=ep.at(em.from); captured=ep.at(em.to+(moved>0?-8:8));
    assert(ep.make(em));
    eb=ea; n->update(ep,em,moved,captured,true,eb); n->refresh(ep,ef); assertAccEqual(eb,ef);

    // Promotion delta.
    Position pr;
    assert(pr.setFEN("4k3/P7/8/8/8/8/8/4K3 w - - 0 1"));
    NNUE::Accumulator pa, pb, pf;
    n->refresh(pr,pa);
    Move pm=findMove(pr,"a7a8q");
    moved=pr.at(pm.from); captured=0;
    assert(pr.make(pm));
    pb=pa; n->update(pr,pm,moved,captured,false,pb); n->refresh(pr,pf); assertAccEqual(pb,pf);

    std::remove(good);
    std::cout << "v0.32 NNUE inference tests: PASS\n";
    return 0;
}
