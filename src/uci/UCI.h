#pragma once
#include "../chess/Position.h"
#include "../engine/Search.h"
#include "../book/OpeningBook.h"
#include <atomic>
#include <thread>
namespace uc {
class UCI{
    Position p_; Search s_; OpeningBook book_; bool useBook_=true; bool nnueEmbedded_=true; std::string bookFile_="book.txt";
    std::thread searchThread_;
    int moveOverheadMs_=30;
    int slowMover_=100;
    std::atomic<bool> searching_{false};
    std::atomic<bool> cancelRequested_{false};
    std::atomic<bool> suppressSearchResult_{false};
    void position(const std::string&);
    void setoption(const std::string&);
    void waitSearch(bool requestStop, bool suppressBestmove = false);
    void startSearch(const std::string& line);
    void emitSearchResult(const Move& m);
public:
    UCI();
    ~UCI();
    void loop();
};
}
