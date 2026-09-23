# ChessZero v1.0.4 — validation record

Date: 2026-09-22

## Engine/API validation

The release was configured as a native C++17 build and the following focused tests were executed:

- `v102_position_king_capture` — PASS
- `v104_engine_api` — PASS
- v1.0.4 JNI-facing native shared library — host-linked successfully against JNI headers

The v1.0.4 API regression covers:

- FEN acceptance and position replacement
- HalfKP network loading
- Threads and Hash configuration
- depth search returning a legal UCI move and non-zero node count
- stop requested from another thread
- C ABI create/set-position/search/destroy path
- idle wait before native handle destruction

## Native JNI surface

The host build produced `libchesszero_android.so` and exported the expected JNI symbols:

- `nativeCreate`
- `nativeDestroy`
- `nativeSetPosition`
- `nativeLoadNetwork`
- `nativeSetThreads`
- `nativeSetHashMb`
- `nativeClearHash`
- `nativeSearch`
- `nativeStop`
- `nativeWaitIdle`
- `nativeIsSearching`
- `nativeLastError`

## Full-suite status

A full `cmake --build` + parallel CTest pass was started. The environment terminated the build after 180 seconds while compiling the historical v0.78/v0.79 test targets. This is an environment time limit, not a reported test failure. The focused release/API tests completed successfully.

## APK status

No final APK is claimed from this environment. The Android SDK/NDK/Gradle stack is not installed here. The project contains the Gradle Android app shell and a separate Android CMake target ready for Android Studio or a configured CI runner.

## Android 16 KB profile

The Android CMake target explicitly links with 16 KB ELF page-size alignment flags. The Gradle app is configured for uncompressed JNI libraries and arm64-v8a. NDK r28+ is recommended.
