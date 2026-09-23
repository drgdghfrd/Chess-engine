# ChessZero v1.0.7 — validation status

## Completed in this source package

- Engine release metadata synchronized to `1.0.7`.
- Android `versionCode=107`, `versionName=1.0.7`.
- Syzygy matrix fixture no longer treats one-byte placeholder files as valid Fathom tables; real probing is optional through `CHESSZERO_SYZYGY_TEST_PATH`.
- Gradle launcher scripts delegate to the official `gradle-wrapper.jar` via `org.gradle.wrapper.GradleWrapperMain`.
- Gradle 8.7 distribution SHA-256 is pinned.
- Vendored Fathom is pinned to commit `c6cf6e8f2f4275e91e03c3612b8d17fe64253c5e`; the source verifier checks Git blob SHA-1 + size and includes the MIT license.
- Checked-in `build-linux/` build artifacts were removed from the release tree.

## Host validation executed

- Release engine configured and built with real vendored Fathom: PASS.
- UCI identity reports `ChessZero v1.0.7`: PASS.
- `tb status` smoke: PASS.
- v0.93 real Fathom backend API test: PASS.
- v1.0.7 Syzygy discovery/eligibility/runtime-guard matrix: PASS.
- v1.0.7 release metadata test: PASS.
- v0.97 UCI hardening test: PASS.
- v1.08 Android game API host test: PASS.
- Android release packaging/static contract tests: PASS.
- Fathom vendored-source verification: PASS.

## Not locally executable in this environment

A real Android APK build requires Android SDK Platform 35, NDK `29.0.14206865`, CMake `3.22.1`, JDK 17, and a working Gradle 8.7 distribution cache/network path. This container currently has no Android SDK/NDK/ADB and has JDK 21 only, so `android/build_apk.sh` stops at the SDK preflight before Gradle can build the APK.

A physical Chessis/OEX device smoke test also requires an attached Android device visible through `adb`; no such tool/device is available in this environment.

Therefore this package does **not** claim an APK build or physical Chessis/OEX run as completed.

## GitHub Actions release workflow

The checked-in workflow uses the pinned Android toolchain (SDK Platform 35, Build Tools 35.0.0, NDK 29.0.14206865, CMake 3.22.1, JDK 17), executes the standard Gradle Wrapper, validates the APK contents and 16 KB ELF/ZIP alignment, and uploads the resulting release APK as an immutable workflow artifact.

The workflow does not claim a physical Chessis/device run; that remains a hardware validation step after the APK artifact is produced.
