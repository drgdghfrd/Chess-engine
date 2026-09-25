# ChessZero v1.0.6 — Android OEX integration

ChessZero now contains the Android pieces required for Open Exchange (OEX) engine discovery:

- `android/app/src/main/res/xml/enginelist.xml` advertises `libchesszero.so` for `arm64-v8a`.
- `MainActivity` exposes the `intent.chess.provider.ENGINE` marker and its OEX provider authority metadata.
- `ChessEngineProvider` serves `libchesszero.so` from APK assets through a read-only content URI.
- `chesszero_oex` is a separate Android PIE UCI executable. It is deliberately not the JNI shared library used by the standalone app.

This distinction matters because OEX consumers copy the engine file and launch it as a UCI process. A JNI-only shared object cannot replace that executable role. The historical Open Exchange support library uses the same discovery pattern: an activity marker, provider authority metadata, an `enginelist.xml`, and an engine file exposed through a `ContentProvider`.

## Standalone app

The app is now a playable technical GUI rather than a FEN-only harness. It provides an 8×8 board, touch/drag moves, human White vs ChessZero Black, New Game, Undo Turn, Stop, engine settings, and a FEN load path for analysis/testing.

## Build requirements

The Android build is pinned to: SDK Platform 35, target SDK 35, NDK 29.0.14206865, and CMake 3.22.1. The Android Gradle Plugin/Gradle wrapper remains pinned as described in `docs/BUILD.md`.

## Syzygy / Fathom

Fathom remains pinned to commit `c6cf6e8f2f4275e91e03c3612b8d17fe64253c5e`. Android CMake first checks `CHESSZERO_FATHOM_SOURCE_DIR`; if the directory contains `src/tbprobe.c`, the configuration is offline with respect to Fathom. If it is absent, CMake FetchContent downloads the pinned source.

An explicit `CHESSZERO_FATHOM_OFFLINE=ON` mode now fails fast instead of attempting network access. This is useful for air-gapped CI or reproducible build environments that preload Fathom.

## Compatibility hardening

The OEX executable asset is stored uncompressed in the APK because Android `AssetManager.openFd()` requires uncompressed assets. `ChessEngineProvider` also implements `openFile()` and a compressed-asset cache fallback for OEX consumers that use `openFileDescriptor()` or receive a non-conforming package.
