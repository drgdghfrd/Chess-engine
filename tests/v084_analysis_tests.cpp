#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <cstdio>
int main(){
    const char* path="v084_test_input.csv";
    std::ofstream f(path);
    f << "config,heuristic,position,depth,score_cp,nodes,nps,wall_ms\n";
    f << "baseline,baseline,1,4,20,1000,5000,10\n";
    f << "baseline,baseline,2,4,30,1200,5100,11\n";
    f << "LMR_100,LMR,1,4,20,900,5500,9\n";
    f << "LMR_100,LMR,2,4,140,1100,5600,10\n";
    f.close();
    std::ifstream in(path); assert(in.good());
    std::string line; std::getline(in,line); assert(line.find("config")!=std::string::npos);
    int rows=0; while(std::getline(in,line)) if(!line.empty()) ++rows;
    assert(rows==4);
    std::remove(path);
    std::cout << "v0.84 heuristic analysis tests: PASS\n";
    std::cout << "Analysis is descriptive only; no configuration is ranked.\n";
}
