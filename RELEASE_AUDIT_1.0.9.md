# ChessZero v1.0.9 — Release audit for GitHub APK build

## Scope

This audit covers the source bundle intended for upload to GitHub and Android APK generation. It includes CMake/native builds, the complete CTest regression suite present in the repository, Python/shell validation, Android/OEX packaging contracts, NNUE file integrity, UCI default loading, and release metadata.

## Fixed during audit

1. Release identity was aligned to v1.0.9/versionCode 109, including the v1.0 release metadata test and Android documentation.
2. Android embedding was changed to use one source of truth: `nets/ChessZero-v1.0.9-current.nnue`. The duplicate raw NNUE asset under `android/app/src/main/assets/` was removed.
3. UCI now advertises and defaults to `embedded://ChessZero-v1.0.9-current.nnue` and reports the loaded NNUE architecture dynamically.
4. The Android release build preflight requires JDK 17 and NDK 29.0.14206865; release signing falls back to the debug key only when no private release keystore is configured, keeping CI artifacts installable for testing.
5. Missing `src/tablebase/Syzygy.cpp` links were fixed in the v0.53 and v0.98 test targets.
6. Several historical tests used paths such as `../nets/...` while CTest executes from the repository root; those paths were corrected and the affected CTest working directories were pinned.
7. The v0.87 shared-TT test had an invalid assertion against a private TT's concurrency counter; the check was moved to the explicitly shared TT portion of the test.
8. The v0.88 parallel-search test compared two counters with different semantics (`worker launches` versus `root tasks completed`); the fabricated ratio assertion was removed while retaining real progress/legality assertions.
9. The bundled Multi-Layer trainer backpropagation order was corrected so all gradients are computed from the pre-update weights before parameters are updated.
10. The Fathom lock was made internally consistent with the exact vendored snapshot present in this source package. The upstream provenance commit remains recorded, while `SOURCE.lock` records the vendored snapshot's Git blob hashes and byte sizes.
11. Documentation was corrected so the production default is not described as the experimental `multi_1m` model.
12. `.github/workflows/android-native.yml` now includes a native-test job that runs the full CTest suite, Fathom vendor verification, Python/shell checks, and all release/NNUE packaging tests before the Android APK job.

## NNUE production decision

- `nets/ChessZero-v1.0.9-current.nnue` = `nets/ChessZero-v1.0.9-sf250k.nnue` byte-for-byte.
- `nets/ChessZero-v1.0.9-single_1m.nnue` remains available for experiments.
- `nets/ChessZero-v1.0.9-multi_1m.nnue` remains available for experiments, but is **not** promoted to the Android production default.

The supplied 1M Multi-Layer model was independently decoded on a 1,000-record sample from the supplied 1M dataset and produced a constant `35.05859375 cp` output for all sampled records. That collapse is inconsistent with a healthy position-dependent evaluation network, so the model is kept out of the production `current.nnue` until the dataset/label pipeline and training are corrected and an actual playing-strength match is run.

## Verified network hashes

- Single 1M (`CZNNUE32`, 10,486,564 bytes):
  `3beda7cab00de94d986e4eeea8b6cb63f4bdc68f0baa678a139a84cea5f1d969`
- Multi 1M (`CZNNUE64`, 10,490,852 bytes):
  `60db37e70587e480c68c0cef46da4a913d7fe8a2ae118ca92d812d6d19855a01`
- Production current/sf250k (`CZNNUE32`, 10,486,564 bytes):
  `ffa001384824c40f1a4d575bb87266abf0e54bb30a300023e1ac7a9abb66966e`

## Test results

- Native CMake build: completed successfully for all configured test targets.
- CTest: **47/47 passed**.
- Python compilation: all `tools/` and `tests/` Python sources compile successfully.
- Shell syntax: Android, OEX, match and helper scripts pass `bash -n`.
- v1.0.8 Android/OEX integration: 22/22 checks passed.
- v1.1.0 NNUE/OEX packaging: 17/17 checks passed.
- v1.1.1 1M NNUE integrity: passed.
- v1.0.9 release packaging: passed.
- Gradle wrapper validation: passed.
- Vendored Fathom verification: passed.
- Host UCI: v1.0.9 identity and `EvalFile` loading were exercised; the experimental Multi-Layer file loads as `HalfKAv2-512x8x32`.

## Android build status

The actual Android APK was **not** built inside this audit container because the required Android SDK/NDK and JDK 17 are not installed here. The GitHub Actions workflow is configured to install platform 35, build-tools 35.0.0, NDK 29.0.14206865, CMake 3.22.1 and JDK 17, then build and publish the release APK.

After pushing this package to GitHub, the expected artifact name is `chesszero-android-release`.
