# UltraChess v0.14

## Search / processing upgrades

1. More Stockfish-style iterative deepening with aspiration-window widening.
2. Stronger move ordering: TT move, MVV-LVA captures, promotions, killers, history.
3. Realer SEE-style exchange simulation for selective capture pruning.
4. Null-move pruning now has a verification search at high depth.
5. Razoring and reverse-futility/static pruning near the horizon.
6. Quiet history bonus/malus updates to reduce repeated bad moves.
7. Check extension: checking moves receive one extra ply.
8. Quiescence delta pruning for obviously hopeless captures.
9. Mate-distance alpha/beta boundaries.
10. UCI Hash and Clear Hash options.
11. TT can be resized at runtime.
12. Android OEX packaging remains static-libstdc++/no-Termux-RUNPATH oriented.

## Important

The v0.14 search is intentionally conservative. It is not claimed to be
stronger than Stockfish without an actual engine-vs-engine tournament.

## Build for Termux OEX

From the engine root:

    ./build_termux_oex.sh

or:

    rm -rf build-termux
    mkdir build-termux
    cd build-termux
    cmake .. -DCMAKE_BUILD_TYPE=Release -DULTRACHESS_ANDROID_OEX=ON
    cmake --build . --target ultrachess -j$(nproc)

Then check:

    file ./libultrachess.so
    readelf -d ./libultrachess.so

The APK project is under:

    android-oex/

The OEX executable must be:

    android-oex/app/src/main/jniLibs/arm64-v8a/libultrachess.so

## Regression tests

Keep the existing perft suite:

    startpos d4      = 197281
    endgame d4        = 43238
    Kiwipete d3       = 97862

Run:

    cmake --build build-termux --target ultrachess-perft
    ./build-termux/ultrachess-perft
