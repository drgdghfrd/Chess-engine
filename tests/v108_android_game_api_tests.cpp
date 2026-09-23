#include "api/EngineAPI.h"

#include <cassert>
#include <iostream>
#include <string>

int main() {
    cz::Engine engine;
    assert(engine.currentFEN().find(" w KQkq - 0 1") != std::string::npos);
    assert(engine.makeMoveUCI("e2e4"));
    assert(engine.currentFEN().find(" b KQkq e3 0 1") != std::string::npos);
    assert(engine.makeMoveUCI("e7e5"));
    assert(engine.currentFEN().find(" w KQkq e6 0 2") != std::string::npos);
    assert(engine.undoMove());
    assert(engine.currentFEN().find(" b KQkq e3 0 1") != std::string::npos);
    assert(engine.undoMove());
    assert(engine.currentFEN().find(" w KQkq - 0 1") != std::string::npos);
    assert(!engine.makeMoveUCI("e2e5"));
    std::cout << "v1.08 Android game API tests: PASS\n";
    return 0;
}
