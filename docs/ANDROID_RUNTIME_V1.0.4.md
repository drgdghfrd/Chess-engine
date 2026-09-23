# ChessZero v1.0.4 — Android runtime layer

v1.0.4 adds a small, stable C ABI plus a JNI facade and a minimal Android application shell.
The UI never calls the native search on the main thread: searches are submitted to a single
background executor and `nativeStop()` can interrupt the native search via the existing atomic
search stop flag.

## Native API

The C-facing entry points live in `src/api/EngineAPI.h` and `src/api/EngineAPI.cpp`.
The API is intentionally opaque (`CZEngine*`) so the Kotlin side does not depend on C++ ABI details.

Supported calls:

- create/destroy
- load NNUE network by filesystem path
- set FEN
- set Threads / Hash MB
- clear hash
- start synchronous search
- stop running search
- query busy state and last error

A search returns a compact pipe-delimited record:

`bestmove|e2e4|depth|8|score|23|nodes|123456|pv|e2e4 e7e5 ...`

## Android packaging

The sample APK shell uses only `arm64-v8a`, packages the HalfKP net as an asset,
copies it to `filesDir` on first run, and loads it through the native API.

The project uses AGP 8.5.1+, targetSdk 35, and uncompressed native libraries. Android's
current 16 KB page-size guidance recommends AGP 8.5.1+ plus NDK r28+ for default 16 KB
compatibility; the CMake file also keeps explicit 16 KB linker flags for older NDKs.

## Build on a machine with Android SDK/NDK

Open `android/` in Android Studio and set the SDK/NDK versions through the IDE. The native
module is rooted at `android/CMakeLists.txt`; the Gradle app uses that file through
`externalNativeBuild.cmake`.

For a command-line debug build, install Gradle and run:

```bash
cd android
gradle :app:assembleDebug
```

The current development container does not ship a complete Android SDK/NDK/Gradle toolchain,
so this repository revision does not claim that an APK was built here.

## ABI / thermal profile

The default sample uses one native engine thread and 32 MB hash to keep the baseline conservative
for mobile. The UI can later expose a battery-aware thread cap without changing the engine ABI.
