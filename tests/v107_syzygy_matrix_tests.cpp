#include "../src/tablebase/Syzygy.h"
#include <cassert>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace uc;

struct Case {
    int pieces;
    const char* fen;
};

static void expect_position(Position& p, const Case& tc) {
    assert(p.setFEN(tc.fen));
    const auto s = Syzygy().status(p);
    assert(s.pieces == tc.pieces);
}

int main() {
    const std::string fixtureRoot = "v107_tb_matrix";
    const std::string nested = fixtureRoot + "/nested";
    std::filesystem::remove_all(fixtureRoot);
    std::filesystem::create_directories(nested);
    // These files are discovery fixtures only. They deliberately are not valid Syzygy
    // tables; real Fathom must reject them rather than claiming probe support.
    std::ofstream(nested + "/KQvK.rtbw").put('x');
    std::ofstream(nested + "/KQvK.rtbz").put('x');

    const std::vector<Case> cases = {
        {3, "7k/8/8/8/8/8/6Q1/6K1 w - - 0 1"},
        {4, "7k/8/8/8/8/6r1/6Q1/6K1 w - - 0 1"},
        {5, "7k/8/8/8/5r2/6Q1/5R2/6K1 w - - 0 1"},
        {6, "7k/8/8/5b2/5r2/6Q1/5R2/6K1 w - - 0 1"},
        {7, "7k/8/8/5b2/5r2/4n1Q1/5R2/6K1 w - - 0 1"},
        {8, "7k/8/8/5b2/5r2/4n1Q1/5R2/5BK1 w - - 0 1"},
    };

    Syzygy tb;
    tb.setPath(fixtureRoot + ":" + nested);
    tb.setProbeLimit(7);

    for (const auto& tc : cases) {
        Position p;
        expect_position(p, tc);
        const auto s = tb.status(p);
        assert(s.pieces == tc.pieces);
        assert(s.configured && s.pathExists);
        assert(s.wdlFiles && s.dtzFiles);
        assert(s.eligible == (tc.pieces >= 2 && tc.pieces <= 7));
// The fixture only validates path/file discovery and the public eligibility matrix.
        // A real Fathom backend must not treat one-byte placeholders as valid tablebases.
        assert(!s.probingAvailable);
        const auto r = tb.probe(p);
        assert(!r.wdlAvailable);
    }

    // Fathom must reject WDL probing when the 50-move clock is non-zero.
    Position halfmove;
    assert(halfmove.setFEN("7k/8/8/8/8/8/6Q1/6K1 w - - 12 1"));
    const auto hs = tb.status(halfmove);
    assert(hs.pieces == 3 && hs.eligible);
#ifdef CHESSZERO_FATHOM
    assert(!tb.probe(halfmove).wdlAvailable);
#endif

    // Castling rights also make the position ineligible for Fathom probing at runtime.
    Position castling;
    assert(castling.setFEN("4k2r/8/8/8/8/8/8/4K2R w Kk - 0 1"));
    assert(tb.status(castling).pieces == 4);
#ifdef CHESSZERO_FATHOM
    assert(tb.status(castling).eligible);
    assert(!tb.probe(castling).wdlAvailable);
#endif

#ifdef CHESSZERO_FATHOM
    // Optional real-table probe: set CHESSZERO_SYZYGY_TEST_PATH to a valid Syzygy
    // directory in CI/device validation. The source tree does not carry large tablebases.
    if (const char* realPath = std::getenv("CHESSZERO_SYZYGY_TEST_PATH")) {
        Syzygy realTb;
        realTb.setPath(realPath);
        realTb.setProbeLimit(7);
        Position rootPos;
        assert(rootPos.setFEN("7k/8/8/8/8/8/6Q1/6K1 w - - 0 1"));
        const auto rs = realTb.status(rootPos);
        assert(rs.eligible && rs.pathExists);
        const auto pr = realTb.probe(rootPos);
        assert(pr.wdlAvailable);
        assert(pr.wdl == SyzygyWDL::Win);
        Move best{};
        SyzygyProbeResult rr{};
        assert(realTb.rootProbe(rootPos, best, rr));
        assert(rr.wdlAvailable);
        assert(rr.rootMoveAvailable);
    }
#endif

    std::filesystem::remove_all(fixtureRoot);
    std::cout << "v1.07 Syzygy 3-7 piece matrix/discovery guards: PASS\n"
              << "matrix=3,4,5,6,7,8\n"
              << "path_list=multi-directory\n"
              << "runtime_guards=halfmove,castling\n";
    return 0;
}
