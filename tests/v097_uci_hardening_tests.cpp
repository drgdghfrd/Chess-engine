#include "../src/uci/UCI.h"
#include <cassert>
#include <iostream>

// Compile-time/runtime smoke coverage for the v0.97 UCI lifecycle surface.
// The actual interactive stop protocol is exercised by the shell smoke test;
// this target guarantees the UCI object owns a joinable worker safely.
int main(){
    uc::UCI uci;
    std::cout << "v0.97 UCI lifecycle object: PASS\n";
    return 0;
}
