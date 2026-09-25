#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ANDROID_DIR="$ROOT/android"

: "${ANDROID_SDK_ROOT:=${ANDROID_HOME:-}}"
if [[ -z "${ANDROID_SDK_ROOT}" ]]; then
  echo "error: ANDROID_SDK_ROOT/ANDROID_HOME is not set" >&2
  exit 2
fi

export ANDROID_SDK_ROOT

# Release build contract: JDK 17 + Android NDK 29.0.14206865.
if [[ -z "${JAVA_HOME:-}" ]]; then
  if command -v java >/dev/null 2>&1; then
    JAVA_HOME="$(dirname "$(dirname "$(readlink -f "$(command -v java)")")")"
    export JAVA_HOME
  fi
fi
if [[ -z "${JAVA_HOME:-}" || ! -x "$JAVA_HOME/bin/java" ]]; then
  echo "error: JDK 17 is required (set JAVA_HOME)" >&2
  exit 2
fi
JAVA_MAJOR="$($JAVA_HOME/bin/java -version 2>&1 | sed -n 's/.*version "\([0-9]*\).*/\1/p' | head -n 1)"
if [[ "$JAVA_MAJOR" != "17" ]]; then
  echo "error: JDK 17 is required; found Java ${JAVA_MAJOR:-unknown}" >&2
  exit 2
fi

SDKMANAGER="$ANDROID_SDK_ROOT/cmdline-tools/latest/bin/sdkmanager"
if [[ ! -x "$SDKMANAGER" ]]; then
  SDKMANAGER="$ANDROID_SDK_ROOT/cmdline-tools/bin/sdkmanager"
fi

if [[ -x "$SDKMANAGER" ]]; then
  "$SDKMANAGER" "platforms;android-35" "build-tools;35.0.0" "ndk;29.0.14206865" "cmake;3.22.1" >/dev/null
fi

NDK_DIR="$ANDROID_SDK_ROOT/ndk/29.0.14206865"
if [[ ! -f "$NDK_DIR/build/cmake/android.toolchain.cmake" ]]; then
  echo "error: Android NDK 29.0.14206865 is required at $NDK_DIR" >&2
  exit 2
fi

cd "$ANDROID_DIR"

EXTRA_ARGS=()
if [[ -f keystore.properties ]]; then
  : "${CHESSZERO_SIGNING_STORE_PASSWORD:?set CHESSZERO_SIGNING_STORE_PASSWORD}"
  : "${CHESSZERO_SIGNING_KEY_ALIAS:?set CHESSZERO_SIGNING_KEY_ALIAS}"
  : "${CHESSZERO_SIGNING_KEY_PASSWORD:?set CHESSZERO_SIGNING_KEY_PASSWORD}"
  EXTRA_ARGS+=(
    "-PchesszeroSigningStoreFile=release-keystore.jks"
    "-PchesszeroSigningStorePassword=${CHESSZERO_SIGNING_STORE_PASSWORD}"
    "-PchesszeroSigningKeyAlias=${CHESSZERO_SIGNING_KEY_ALIAS}"
    "-PchesszeroSigningKeyPassword=${CHESSZERO_SIGNING_KEY_PASSWORD}"
  )
fi

bash ./gradlew :app:assembleRelease \
  -PchesszeroFathomOffline=true \
  --no-daemon \
  "${EXTRA_ARGS[@]}"

APK="app/build/outputs/apk/release/app-release.apk"
if [[ ! -f "$APK" ]]; then
  APK="$(find app/build/outputs/apk/release -maxdepth 1 -type f -name "*.apk" | sort | head -n 1)"
fi

if [[ -z "$APK" || ! -f "$APK" ]]; then
  echo "error: release APK was not produced" >&2
  exit 3
fi

echo "Release APK: $ANDROID_DIR/$APK"
