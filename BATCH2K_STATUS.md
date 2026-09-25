# ChessZero v1.0.6 — Batch 2K Status

## Goal

Turn the Android project from a technical harness into a usable chess app and restore Open Exchange (OEX) engine interoperability for Chessis-class Android GUIs.

## Implemented

- Added a real `ChessBoardView` with touch/drag movement and board rendering.
- Added human-White vs engine-Black play flow.
- Added New Game, Undo Turn, Stop, engine settings, and FEN load/setup.
- Extended `EngineAPI` with `newGame`, legal UCI move application, undo, and current-FEN access.
- Added `ChessEngineProvider` and OEX manifest discovery metadata.
- Added `res/xml/enginelist.xml` advertising `libchesszero.so` for `arm64-v8a`.
- Added a separate `chesszero_oex` Android PIE executable target.
- The OEX executable is copied into generated APK assets as `libchesszero.so`; it is intentionally separate from `libchesszero_android.so` (JNI).
- Pinned Android SDK Platform 35, target SDK 35, NDK `29.0.14206865`, and CMake `3.22.1`.
- Added explicit Fathom source-dir/offline controls.
- Added native game API regression tests and Android/OEX static integration tests.

## Verification

- `tests/v108_android_game_api_tests.cpp`: **PASS**.
- `tests/v108_android_integration_tests.py`: **PASS (15 checks)**.
- Host OEX configuration with `CHESSZERO_ANDROID_OEX=ON`: **PASS**; generated artifact is an ELF PIE executable (`ET_DYN`) and answers UCI commands.
- Host UCI smoke test: **PASS**.
- Full Android APK build: **NOT CLAIMED** in this container because Android SDK Platform 35, NDK `29.0.14206865`, and CMake `3.22.1` are not installed here.

## Remaining limitation

The repository does not yet vendor the complete Fathom source tree under `third_party/Fathom/`; therefore a clean online build still fetches the pinned Fathom commit unless a local `CHESSZERO_FATHOM_SOURCE_DIR` is supplied. Batch 2L should make the source archive fully self-contained by vendoring that pinned source tree and then run a real Android arm64 release build.
