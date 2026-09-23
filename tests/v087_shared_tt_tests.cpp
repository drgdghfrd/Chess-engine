#include "../src/engine/Search.h"
#include "../src/engine/Evaluation.h"
#include "../src/chess/Position.h"
#include <cassert>
#include <iostream>
#include <thread>
#include <vector>

using namespace uc;

int main(){
    assert(network().load("nets/ChessZero-v0.75-halfkp.nnue"));
    Position p;
    p.start();

    Search s(32);
    s.setThreads(4);
    Move m=s.go(p,5);
    assert(m.from<64 && m.to<64);
    assert(s.completedDepth()>=1);
    assert(s.lazySmpLaunches()>0);
    assert(s.lazySmpCompleted()>0);
    assert(s.sharedTTOps()>0);

    // Exercise the same TT concurrently through independent Search objects.
    // They share one table exactly as Lazy SMP workers do.
    TranspositionTable shared(8);
    std::vector<std::thread> ts;
    std::atomic<int> ok{0};
    for(int i=0;i<4;i++){
        ts.emplace_back([&](){
            Search w(8, &shared);
            w.setThreads(1);
            Position q;
            q.start();
            Move x=w.go(q,3);
            if(x.from<64 && x.to<64) ok.fetch_add(1);
        });
    }
    for(auto& t:ts)t.join();
    assert(ok==4);
    assert(shared.concurrentOps()>0);

    std::cout << "v0.87 shared TT tests: PASS\n"
              << "lazy_launches=" << s.lazySmpLaunches()
              << " lazy_completed=" << s.lazySmpCompleted()
              << " shared_tt_ops=" << s.sharedTTOps()
              << " concurrent_workers=" << ok.load() << "\n";
    return 0;
}
