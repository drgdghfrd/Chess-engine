#pragma once
#include "../chess/Position.h"
#include "../engine/Search.h"
#include "../book/OpeningBook.h"
namespace uc {
class UCI{
    Position p_; Search s_; OpeningBook book_; bool useBook_=true;
    void position(const std::string&);
    void setoption(const std::string&);
public: UCI(); void loop();
};
}
