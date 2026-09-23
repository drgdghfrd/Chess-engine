#include "../src/book/OpeningBook.h"
#include "../src/chess/Position.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <filesystem>
using namespace uc;
static void be16(std::ofstream&f,unsigned v){ f.put(char(v>>8)); f.put(char(v)); }
static void be32(std::ofstream&f,unsigned v){ f.put(char(v>>24));f.put(char(v>>16));f.put(char(v>>8));f.put(char(v)); }
static void be64(std::ofstream&f,uint64_t v){ for(int i=7;i>=0;--i)f.put(char(v>>(8*i))); }
int main(){
    std::filesystem::path path="v093_test.bin";
    { std::ofstream f(path,std::ios::binary); be64(f,0x0123456789abcdefULL); be16(f,uint16_t(12<<6|28)); be16(f,7); be32(f,123); be64(f,0x0123456789abcdefULL); be16(f,uint16_t(11<<6|27)); be16(f,3); be32(f,456); }
    OpeningBook b; assert(b.load(path.string())); assert(b.loaded()); assert(b.format()==BookFormat::Polyglot); assert(b.size()==2); assert(std::string(b.formatName())=="polyglot");
    Position p; p.start(); assert(b.probePolyglotKey(p,0x0123456789abcdefULL).from==12);
    std::filesystem::remove(path);
    std::cout<<"v0.93 PolyGlot binary reader tests: PASS\nentries=2 format=polyglot weighted_probe=ok\n";
}
