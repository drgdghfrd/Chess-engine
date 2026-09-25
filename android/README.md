# ChessZero Android runtime

v1.0.9 ships a minimal Android application shell, JNI bridge and a self-contained arm64-v8a native engine.
It is deliberately small: no AndroidX dependency and no UI work is performed in the native search thread.

## Layout

```text
android/
├── CMakeLists.txt
├── build.gradle.kts
├── settings.gradle.kts
├── gradle.properties
├── gradlew
├── gradlew.bat
├── gradle/wrapper/
├── build_apk.sh
└── app/
    ├── build.gradle.kts
    └── src/main/
        ├── AndroidManifest.xml
        ├── cpp/jni_bridge.cpp
        └── java/com/chesszero/app/
            ├── MainActivity.kt
            └── NativeChessZero.kt
```

## Runtime design

```text
MainActivity
    │  background ExecutorService
    ▼
NativeChessZero.kt
    │ JNI
    ▼
C ABI: src/api/EngineAPI.*
    ▼
Position + Search + NNUE + TT
```

The Kotlin side owns the UI lifecycle. `search()` is submitted to a single background executor;
`stop()` only sets the existing atomic search-stop flag, so it does not block the UI. Native state
changes are rejected while a search is active. Destruction calls `stop()` and waits for idle before
releasing the native handle.

The sample copies the packaged HalfKP network into app-private `filesDir` on first launch and loads
it from there. This avoids teaching the hot engine loop about Android `AssetManager` paths.

## Build

Open `android/` in Android Studio with an installed Android SDK, NDK and CMake. The app uses
AGP 8.5.1+, targetSdk 35 and arm64-v8a only. AGP 8.5.1+ is used so the native library packaging path is compatible with current 16 KB guidance. A CLI release build should use `./gradlew :app:assembleRelease`; the standard Gradle Wrapper resolves the pinned Gradle 8.7 distribution.

The development container used for the release does not contain a complete Android SDK/NDK/Gradle
installation, so this release does not claim a locally-produced APK.

## Release validation

The source release includes static APK/OEX validation helpers under `tools/android/`. A physical-device
Chessis/OEX smoke test is required to validate installation, engine discovery and UCI launch on hardware.

## 16 KB page-size support

The native CMake build explicitly requests 16 KB ELF page alignment. Android's current guidance
says AGP 8.5.1+ with NDK r28+ provides default 16 KB compatibility for native apps; the explicit
linker flags are retained as an additional release guard. See the official Android guidance:
https://developer.android.com/guide/practices/page-sizes

## Mobile defaults

The sample defaults to 1 thread and 32 MB hash, with UI fields allowing up to 8 threads and 512 MB
hash. This is intentionally conservative until thermal and battery measurements are collected on a
real device. There is no automatic frequency manipulation or GPU compute path.

## Deterministic Fathom build

For a release/offline build, populate `../third_party/Fathom` first with
`../tools/vendor_fathom.sh`. The Android CMake layer defaults to that vendored
tree and refuses FetchContent when the vendored-source requirement is enabled.
`CHESSZERO_FATHOM_OFFLINE=ON` is the recommended release mode so a missing
Fathom source fails immediately instead of accessing the network.

After an APK is built, run:

```text
../tools/android/validate_apk.sh app/build/outputs/apk/release/app-release.apk
```

This verifies the OEX executable asset, arm64 JNI library, ET_DYN/PIE type, and
per-LOAD-segment alignment suitable for 16 KB page-size devices.
