# ChessZero v1.0 Build Guide

## Linux / macOS / Termux

Requirements: CMake 3.15+ and a C++17 compiler.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2 --target chesszero
```

Run the UCI engine:

```bash
printf 'uci\nisready\nposition startpos\ngo depth 5\nquit\n' | ./build/chesszero
```

Run the test suite:

```bash
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

For a smaller mobile build without regression targets:

```bash
cmake -S . -B build-mobile -DCMAKE_BUILD_TYPE=Release -DCHESSZERO_BUILD_TESTS=OFF
cmake --build build-mobile -j2 --target chesszero
```

## Windows

Open the project from Visual Studio or use a Developer PowerShell:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target chesszero
```

The CMake project uses `Threads::Threads` instead of a hard-coded POSIX pthread
library for the main engine target.

## Android APK / Gradle Wrapper

The Android project lives under `android/` and pins Android Gradle Plugin 8.5.1 with
Gradle 8.7. The repository includes the standard Gradle Wrapper `gradlew` / `gradlew.bat` launchers backed by the checked-in official Wrapper JAR; use them rather than a system Gradle installation.

```bash
cd android
./gradlew :app:assembleRelease
# or
./build_apk.sh
```

The wrapper delegates to `org.gradle.wrapper.GradleWrapperMain` in the official Wrapper JAR. The pinned `distributionSha256Sum` protects the Gradle 8.7 distribution fetched by the Wrapper.

## Android NDK / native engine library

The repository builds two Android native artifacts: the JNI shared library used by
the standalone ChessZero app, and a separate PIE UCI executable packaged as
`libchesszero.so` for Open Exchange (OEX) chess GUIs such as Chessis. With an installed
NDK toolchain:

```bash
./tools/android/build_oex.sh
```

The OEX artifact is an executable, not the JNI shared library. This distinction is
required because OEX consumers copy the engine file and execute it as a UCI process.

### Syzygy / Fathom build modes

Android configuration uses the pinned Fathom commit used by the desktop engine. For
release/offline builds require a local Fathom source tree containing `src/tbprobe.c`.
The Android project defaults to `CHESSZERO_REQUIRE_VENDORED_FATHOM=ON`, so a missing
vendored tree fails closed instead of silently using the network. Developers can explicitly
override that policy when they intentionally want FetchContent.

The Android project targets compileSdk/targetSdk 35, so a build machine must have
Android SDK Platform 35 installed.

## Installation

CMake installation copies the executable/library plus packaged networks and docs:

```bash
cmake --install build --prefix dist
```
