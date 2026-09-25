# ChessZero v1.0.7 — release validation

Current release surface:

- Engine version: `1.0.7`
- Android `versionCode`: `107`
- Android `versionName`: `1.0.7`
- compileSdk / targetSdk: `35`
- NDK: `29.0.14206865`
- CMake: `3.22.1`
- Gradle: `8.7` via the standard Gradle Wrapper
- ABI: `arm64-v8a`
- Syzygy backend: vendored Fathom, pinned in `third_party/Fathom/SOURCE.lock`

The `v1.0.7` Syzygy matrix test checks discovery/eligibility using placeholder files but deliberately does not treat those placeholders as valid tablebases. Real Fathom probing can be exercised by setting `CHESSZERO_SYZYGY_TEST_PATH` to an actual Syzygy directory.

Android release validation should include: native/OEX build, APK contents and 16 KiB ELF alignment checks, signature/installability, and final Chessis/OEX discovery on a physical Android device when one is available.
