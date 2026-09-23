#include "uci/UCI.h"
#include "Version.h"
#include "bitboard/Bitboard.h"
#include <iostream>

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    uc::BitOps::initAttackTables();
    std::cout << uc::kEngineName << " v" << uc::kVersion << " by Zero (UCI)" << std::endl;

    uc::UCI uci;
    uci.loop();
    return 0;
}
