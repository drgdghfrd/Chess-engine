# UltraChess OEX for Chessis

This module packages UltraChess as an Open Exchange (OEX) Android engine so a compatible GUI can discover it.

The engine name is `UltraChess v0.12` and the OEX filename is `libultrachess.so` for `arm64-v8a`.

## Important

The OEX APK must contain an **actual ARM64 UltraChess UCI executable** at:

`app/src/main/jniLibs/arm64-v8a/libultrachess.so`

Do not rename the Stockfish binary to this name: that would make Chessis run Stockfish while displaying UltraChess.

## Build the native UCI executable

Use Android NDK CMake and build `src/main.cpp` plus the engine sources as an Android executable, with the output file named `libultrachess.so`. The program must remain a stdin/stdout UCI process.

A recommended NDK build target is `aarch64-linux-android24`.

## Build the APK

Open this directory in Android Studio and build a signed APK. Install the APK, then restart Chessis and check its engine selection screen.

## OEX discovery

The manifest advertises `intent.chess.provider.ENGINE`, provides `chess.provider.engine.authority`, and exposes `enginelist.xml`. This follows the established Android OEX pattern used by Chessis-compatible engine packages.
