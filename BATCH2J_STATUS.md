# ChessZero v1.0.6 — Batch 2J Status

## Goal

Make the Android build reproducible from a clean source checkout without requiring a system-installed Gradle.

## Implemented

- Pinned Gradle `8.7` in `android/gradle/wrapper/gradle-wrapper.properties`.
- Pinned the Gradle 8.7 distribution SHA-256.
- Added `android/gradlew` and `android/gradlew.bat`.
- Added a checksum-verified bootstrap for the official Gradle 8.7 wrapper JAR.
- Added `android/build_apk.sh` as a one-command release APK entry point.
- Added Gradle cache/build-state ignores.
- Updated Android build documentation.
- Android CI now has a dedicated wrapper-driven APK build job.

## Toolchain basis

Android Gradle Plugin `8.5.1` requires Gradle `8.7`. Android's AGP 8.5 compatibility guidance specifies JDK 17 as the required/default Java version. Gradle's documentation recommends the Wrapper as the standardized project entry point.

## Verification

Local shell validation completed for the wrapper scripts and Gradle metadata. A full APK build was **not** claimed in this container because outbound access to the Gradle artifact host is unavailable and a complete Android SDK/NDK stack is not present.

On a networked Android/CI environment, the intended command is:

```bash
cd android
./gradlew :app:assembleRelease
```

The wrapper itself is self-bootstrapping: it fetches the official Gradle 8.7 wrapper JAR and validates its SHA-256 before starting Gradle.
