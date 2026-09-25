# GitHub APK build — ChessZero v1.0.9

## What to upload
Upload the **contents of this directory** to the root of your GitHub repository (`drgdghfrd/Chess-engine`). The workflow is already under `.github/workflows/android-native.yml`.

## Production NNUE
`nets/ChessZero-v1.0.9-current.nnue` is the single source of truth for the Android build. It is byte-identical to `nets/ChessZero-v1.0.9-sf250k.nnue` and is embedded directly into both the OEX engine and JNI library. Do **not** copy a raw `.nnue` file into `android/app/src/main/assets/`.

The two new 1M networks stay available for experiments:
- `nets/ChessZero-v1.0.9-single_1m.nnue`
- `nets/ChessZero-v1.0.9-multi_1m.nnue`

## GitHub Actions
A push to `main`/`master`, a pull request, or manual `workflow_dispatch` starts `.github/workflows/android-native.yml`. The APK job installs Android platform 35, build-tools 35.0.0, NDK 29.0.14206865, CMake 3.22.1 and JDK 17, validates the release/package contracts, builds the release APK and publishes `chesszero-android-release`.

## Local UCI experiment
For the production network:
```text
setoption name EvalFile value nets/ChessZero-v1.0.9-current.nnue
```
For the new Multi-Layer experimental network:
```text
setoption name EvalFile value nets/ChessZero-v1.0.9-multi_1m.nnue
```

## Release signing
Without a private keystore, the Gradle release build falls back to the debug signing key so the generated APK remains installable for testing. A configured `release-keystore.jks` plus the four signing properties takes precedence for a private release certificate.
