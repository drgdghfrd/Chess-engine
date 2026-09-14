#include "../src/chess/Position.h"
#include "../src/engine/Search.h"
#include "../src/chess/Move.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>

using namespace uc;

int main(int argc, char** argv) {
    const int games = argc > 1 ? std::max(1, std::stoi(argv[1])) : 1;
    const int depth = argc > 2 ? std::max(1, std::stoi(argv[2])) : 3;
    const std::string out = argc > 3 ? argv[3] : "selfplay.csv";

    std::ofstream csv(out);
    if (!csv) {
        std::cerr << "cannot open output: " << out << '\n';
        return 1;
    }
    csv << "game,ply,fen,side,score,result\n";

    for (int g = 0; g < games; ++g) {
        Position p;
        Search s(32);
        s.setThreads(1);
        int ply = 0;

        while (ply < 300 && !p.over()) {
            const std::string fen = p.fen();
            const int side = p.side();
            Move best = s.go(p, depth);
            if (best.from == best.to && best.from == 0 && best.flag == Flag::Quiet) break;
            csv << g << ',' << ply << ",\"" << fen << "\"," << side << ','
                << s.score() << ",*\n";
            if (!p.make(best)) break;
            ++ply;
        }
    }
    std::cout << "wrote " << out << '\n';
    return 0;
}
