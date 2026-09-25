# ChessZero v1.0.8 — NNUE/OEX self-contained runtime fix

## Problem observed on device

ChessZero v1.0.7 already contained the HalfKP-40960x256 NNUE model in the APK, but the Chessis/OEX execution path exposed only `assets/libchesszero.so` to the external GUI. The OEX process therefore could not rely on the Android app's private asset path for the network.

## v1.0.8 fix

- The release HalfKP network `ChessZero-v0.75-halfkp.nnue` is embedded directly into the arm64-v8a native targets.
- The Android build converts the network into a read-only AArch64 ELF object with LLVM `objcopy` and links that object into both `chesszero_oex` and `chesszero_android`.
- `NNUE::loadMemory()` provides a single validated parser for file-backed and embedded networks.
- `NNUE::loadEmbedded()` loads the packaged network without filesystem access.
- The OEX UCI engine auto-loads the embedded network before filesystem fallbacks.
- `EvalFile` accepts `embedded://ChessZero-v0.75-halfkp.nnue`.
- `NNUEEmbedded` can explicitly enable or disable NNUE evaluation for diagnostics.
- The standalone Android UI uses the same embedded model and no longer copies a 10 MiB network asset at startup.
- The duplicate Android NNUE asset was removed, so the APK does not contain two copies of the same model.

## Validation performed in the development environment

- HalfKP v0.75 network load + inference: PASS.
- Android/OEX integration checks: PASS (19 checks).
- NNUE/OEX embedded packaging checks: PASS (14 checks).
- Gradle wrapper checks: PASS.
- Release metadata checks: PASS; version `1.0.8`, versionCode `108`.
- Android game API tests: PASS.
- Engine API tests: PASS.
- Syzygy matrix tests: PASS.
- Manual self-contained embedded OEX host smoke test: PASS; UCI reports `nnue=loaded` without reading an external network file.
- ASan/UBSan self-contained embedded engine search smoke test: PASS; no sanitizer errors reported.

## Android release build

The development container does not contain the Android SDK/NDK toolchain, so an arm64 Android APK was not built here. GitHub Actions remains the authoritative Android build path. The workflow now checks the embedded NNUE/OEX packaging contract before the Gradle release build.
