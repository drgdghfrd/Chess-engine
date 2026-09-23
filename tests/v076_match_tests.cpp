#include <cassert>
#include <cmath>
#include <iostream>

static double eloDelta(double s){ return 400.0*std::log10(s/(1.0-s)); }

int main(){
    assert(std::abs(eloDelta(0.5)) < 1e-12);
    assert(eloDelta(0.75) > 190.0 && eloDelta(0.75) < 195.0);
    int w=10,d=6,l=4; double s=(w+0.5*d)/20.0;
    assert(std::abs(s-0.65)<1e-12);
    std::cout<<"v076 match statistics tests: PASS\n";
}
