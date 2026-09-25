#include "nnue/NNUE.h"
#include <cstdio>
#include <cstring>
#include <random>
#include <string>

using uc::NNUE;

int main() {
    // Only meaningful on DotProd devices; elsewhere skip cleanly.
    if (std::string(uc::nnueSimdPath()) != "sdot") {
        std::printf("v041_simd_tests: SKIP (simd=%s, need sdot)\n", uc::nnueSimdPath());
        return 0;
    }

    int8_t w[NNUE::HIDDEN_SIZE];
    int16_t a[NNUE::HIDDEN_SIZE];
    const char* names[] = {"all-zero", "all-clamp-high", "random-a", "random-b"};

    for (int c = 0; c < 4; ++c) {
        if (c == 0) {
            std::memset(a, 0, sizeof(a));
        } else if (c == 1) {
            for (auto& v : a) v = 500;  // clamped to 127
        } else {
            std::mt19937 rng(1000 + c);
            std::uniform_int_distribution<int> d(-400, 400);
            for (auto& v : a) v = static_cast<int16_t>(d(rng));
        }

        std::mt19937 wrng(2000 + c);
        std::uniform_int_distribution<int> wd(-128, 127);
        for (auto& v : w) v = static_cast<int8_t>(wd(wrng));

        const int viaMlal = NNUE::forwardPathForTest(a, w, 12345, 8, 0);
        const int viaSdot = NNUE::forwardPathForTest(a, w, 12345, 8, 1);

        if (viaMlal != viaSdot) {
            std::printf("v041_simd_tests: FAIL '%s' mlal=%d sdot=%d\n",
                        names[c], viaMlal, viaSdot);
            return 1;
        }
        std::printf("v041_simd_tests: PASS '%s' (%d)\n", names[c], viaSdot);
    }
    std::printf("v041_simd_tests: 100%% PASS\n");
    return 0;
}
