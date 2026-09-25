#include "api/EngineAPI.h"
#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>

int main() {
    cz::Engine engine;
    assert(engine.setThreads(7));
    assert(engine.setHashMB(64));

    std::thread searcher([&] {
        auto result = engine.search(64, 1500);
        (void)result;
    });

    // Let the search enter its worker path, then request new resources while it
    // is active. The API must stop-and-wait, apply the new values, and return
    // successfully rather than reporting "engine busy".
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    assert(engine.isSearching());
    assert(engine.setThreads(5));
    assert(!engine.isSearching());
    assert(engine.setHashMB(32));

    searcher.join();
    assert(engine.setThreads(2));
    assert(engine.setHashMB(16));

    std::cout << "v1.1.0 EngineAPI runtime reconfiguration: PASS\n";
    return 0;
}
