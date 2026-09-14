# UltraChess v0.14 OEX - Stockfish-style Android build

This package is prepared to build UltraChess as a standalone ARM64 Android OEX
engine executable.

## Why this version is different

The previous binary was linked against `libc++_shared.so` and contained a
Termux RUNPATH. That can work on the development phone but can crash on another
device or OEX host.

This build uses the Android NDK `c++_static` STL and `-static-libstdc++` so the
engine does not require `libc++_shared.so` to be supplied by the OEX host.

## Build

Set `ANDROID_NDK_ROOT`, then run:

    ./android-oex/build_native_arm64.sh

The resulting file is:

    android-oex/app/src/main/jniLibs/arm64-v8a/libultrachess.so

Check it with:

    file android-oex/app/src/main/jniLibs/arm64-v8a/libultrachess.so
    readelf -d android-oex/app/src/main/jniLibs/arm64-v8a/libultrachess.so

For an OEX engine there should be no Termux RUNPATH and no dependency on
`libc++_shared.so`.

## APK

Build the Android app from `android-oex/`. Then install the APK and add
"UltraChess v0.14" from Chessis -> Add OEX engines.

## Important

UltraChess currently uses its own NNUE format. It is not compatible with a
Stockfish `.nnue` file just by copying the file into the APK. Stockfish embeds
its default NNUE network into the engine binary; UltraChess will need its own
embedding step if you want the same model-loading design.
