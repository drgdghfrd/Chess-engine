#include "../src/Version.h"
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

static std::string readFile(const fs::path& p) {
    std::ifstream f(p);
    assert(f && "required release file missing or unreadable");
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

int main() {
    assert(std::string(uc::kVersion) == "1.0.8");
    assert(std::string(uc::kEngineName) == "ChessZero");
    assert(fs::exists("CMakeLists.txt"));
    assert(fs::exists("CHANGELOG.md"));
    assert(fs::exists("docs/UCI.md"));
    assert(fs::exists("docs/BUILD.md"));
    assert(fs::exists("docs/RELEASE.md"));
    assert(fs::exists("tools/release_package.py"));
    assert(fs::exists("tools/android/build_oex.sh"));
    assert(fs::exists("tools/match/run_match.py"));
    assert(fs::exists("tests/v102_match_runner_test.py"));

    const std::string cmake = readFile("CMakeLists.txt");
    assert(cmake.find("project(ChessZero VERSION 1.0") != std::string::npos);
    assert(cmake.find("Threads::Threads") != std::string::npos);
    assert(cmake.find("install(TARGETS chesszero") != std::string::npos);
    assert(cmake.find("v101_match_runner") != std::string::npos);

    const std::string changelog = readFile("CHANGELOG.md");
    assert(changelog.find("## v1.0") != std::string::npos);
    assert(changelog.find("does not claim a new independently measured Elo result") != std::string::npos);

    const std::string uci = readFile("docs/UCI.md");
    assert(uci.find("Move Overhead") != std::string::npos);
    assert(uci.find("go infinite") != std::string::npos);
    assert(uci.find("book probe") != std::string::npos);
    assert(uci.find("tb probe") != std::string::npos);

    std::cout << "v1.0 release metadata: PASS\n"
              << "version=1.0.8 install_surface=true platform_docs=true\n";
    return 0;
}
