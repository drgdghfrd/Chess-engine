#include "../src/book/OpeningBook.h"
#include "../src/book/PolyglotZobrist.h"
#include <cassert>
#include <cstdint>
#include <fstream>
#include <iostream>

using namespace uc;

static void be16(std::ofstream& f, uint16_t v){ char b[2]={char(v>>8),char(v)}; f.write(b,2); }
static void be32(std::ofstream& f, uint32_t v){ char b[4]={char(v>>24),char(v>>16),char(v>>8),char(v)}; f.write(b,4); }
static void be64(std::ofstream& f, uint64_t v){ for(int i=7;i>=0;--i){ char c=char(v>>(i*8)); f.write(&c,1); } }

int main(){
    assert(PolyglotZobrist::isCanonicalTableReady());
    Position p; p.start();
    const uint64_t key=PolyglotZobrist::key(p);
    assert(key==0x463B96181691FC9Cull);

    const char* path="v096_workflow_test.bin";
    {
        std::ofstream f(path,std::ios::binary);
        assert(f);
        // e2e4, PolyGlot raw move: from=e2 (12), to=e4 (28), promo=0.
        uint16_t raw=uint16_t((12u<<6)|28u);
        be64(f,key); be16(f,raw); be16(f,10); be32(f,0);
    }

    OpeningBook book;
    assert(book.load(path));
    assert(book.loaded());
    assert(book.format()==BookFormat::Polyglot);
    assert(book.size()==1);
    Move m=book.probe(p);
    assert(m.from==p.parseSq("e2") && m.to==p.parseSq("e4"));
    assert(book.probePolyglot(p).from==m.from);

    std::remove(path);
    std::cout << "v0.96 opening-book workflow: PASS\n"
              << "polyglot_hash=ready direct_probe=true reloadable_bin=true\n";
}
