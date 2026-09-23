#include "../src/chess/Position.h"
#include "../src/engine/Search.h"
#include "../src/engine/Evaluation.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>

static float teacherTarget(int cp) {
    // Smooth classical teacher target.  This keeps most quiet positions near
    // zero while preserving useful gradients in middlegames/endgames.
    return std::tanh(static_cast<float>(cp) / 600.0f);
}

static const char* gameResult(const uc::Position& p) {
    auto legal = p.legal();
    if (!legal.empty() && !p.isDraw()) return "*";
    if (p.isDraw() || (!legal.empty() && p.isDraw())) return "1/2-1/2";
    // Side to move has no legal move. Checkmate means the other side won.
    return p.inCheck(p.side() == 1) ? (p.side() == 1 ? "0-1" : "1-0") : "1/2-1/2";
}

int main(int argc, char** argv) {
    int games = 20, depth = 3, maxPlies = 80, randomOpening = 6, searchEvery = 4;
    uint32_t seed = 7501;
    std::string teacherOut = "build/data/selfplay_teacher.txt";
    std::string resultOut = "build/data/selfplay_results.txt";
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        auto need = [&](const char* n) -> const char* {
            if (i + 1 >= argc) { std::cerr << "missing value for " << n << "\n"; std::exit(2); }
            return argv[++i];
        };
        if (a == "--games") games = std::stoi(need("--games"));
        else if (a == "--depth") depth = std::stoi(need("--depth"));
        else if (a == "--plies") maxPlies = std::stoi(need("--plies"));
        else if (a == "--random-opening-plies") randomOpening = std::stoi(need("--random-opening-plies"));
        else if (a == "--search-every") searchEvery = std::stoi(need("--search-every"));
        else if (a == "--seed") seed = static_cast<uint32_t>(std::stoul(need("--seed")));
        else if (a == "--teacher-out") teacherOut = need("--teacher-out");
        else if (a == "--result-out") resultOut = need("--result-out");
        else { std::cerr << "unknown argument: " << a << "\n"; return 2; }
    }
    if (games < 1 || depth < 1 || maxPlies < 1 || randomOpening < 0 || searchEvery < 1) return 2;

    std::ofstream teacher(teacherOut), results(resultOut);
    if (!teacher || !results) { std::cerr << "cannot open output\n"; return 1; }
    std::mt19937 rng(seed);
    size_t positions = 0;

    // Intentionally do not load an NNUE network: v0.75 bootstrap self-play
    // uses the mature classical evaluator as its teacher.
    for (int g = 0; g < games; ++g) {
        uc::Position p;
        p.start();
        std::vector<std::string> fens;
        std::vector<int> teacherCp;
        fens.reserve(maxPlies);
        teacherCp.reserve(maxPlies);
        uc::Search search(16);

        for (int ply = 0; ply < maxPlies && !p.over(); ++ply) {
            auto legal = p.legal();
            if (legal.empty()) break;
            fens.push_back(p.fen());
            teacherCp.push_back(uc::classicalEval(p));
            ++positions;

            uc::Move m{};
            if (ply < randomOpening || (ply % searchEvery) != 0) {
                std::uniform_int_distribution<size_t> d(0, legal.size() - 1);
                m = legal[d(rng)];
            } else {
                m = search.go(p, depth);
                if (m.from == m.to && m.from == 0 && m.flag == uc::Flag::Quiet)
                    m = legal.front();
            }
            if (!p.make(m)) break;
        }

        const std::string result = gameResult(p);
        for (size_t i = 0; i < fens.size(); ++i) {
            float t = teacherTarget(teacherCp[i]);
            teacher << fens[i] << '\t' << t << '\t' << "game_" << g << '\n';
        }
        // Result-labelled set: convert the final result into side-to-move POV.
        for (const std::string& fen : fens) {
            uc::Position q;
            q.setFEN(fen);
            float t = 0.0f;
            if (result == "1-0") t = q.side() == 1 ? 1.0f : -1.0f;
            else if (result == "0-1") t = q.side() == -1 ? 1.0f : -1.0f;
            results << fen << '\t' << t << '\t' << "game_" << g << '\n';
        }
        std::cout << "game " << (g + 1) << "/" << games << " result=" << result
                  << " plies=" << fens.size() << "\n";
    }
    std::cout << "generated " << positions << " positions\n";
    std::cout << "teacher: " << teacherOut << "\nresult:  " << resultOut << "\n";
    return 0;
}
