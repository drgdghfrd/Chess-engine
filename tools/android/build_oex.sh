#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
NDK="${ANDROID_NDK_HOME:-${ANDROID_NDK_ROOT:-}}"
ABI="${ANDROID_ABI:-arm64-v8a}"
PLATFORM="${ANDROID_PLATFORM:-android-24}"
BUILD_DIR="${CHESSZERO_ANDROID_BUILD_DIR:-build-android}"
LTO="${CHESSZERO_ENABLE_LTO:-OFF}"
SYZYGY="${CHESSZERO_ENABLE_SYZYGY:-ON}"
ANDROID_16K="${CHESSZERO_ANDROID_16K:-ON}"
FATHOM_DIR="${CHESSZERO_FATHOM_SOURCE_DIR:-$ROOT/third_party/Fathom}"
FATHOM_OFFLINE="${CHESSZERO_FATHOM_OFFLINE:-ON}"

if [[ -z "$NDK" || ! -f "$NDK/build/cmake/android.toolchain.cmake" ]]; then
  echo "error: set ANDROID_NDK_HOME (or ANDROID_NDK_ROOT) to an installed Android NDK" >&2
  exit 2
fi

cmake -S "$ROOT/android" -B "$ROOT/$BUILD_DIR" \
  -DCMAKE_TOOLCHAIN_FILE="$NDK/build/cmake/android.toolchain.cmake" \
  -DANDROID_ABI="$ABI" \
  -DANDROID_PLATFORM="$PLATFORM" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCHESSZERO_ENABLE_LTO="$LTO" \
  -DCHESSZERO_ENABLE_SYZYGY="$SYZYGY" \
  -DCHESSZERO_ANDROID_16K="$ANDROID_16K" \
  -DCHESSZERO_FATHOM_SOURCE_DIR="$FATHOM_DIR" \
  -DCHESSZERO_FATHOM_OFFLINE="$FATHOM_OFFLINE" \
  -DCHESSZERO_REQUIRE_VENDORED_FATHOM="$FATHOM_OFFLINE"

cmake --build "$ROOT/$BUILD_DIR" --target chesszero_oex -j2

LIB="$ROOT/$BUILD_DIR/libchesszero.so"
if [[ ! -f "$LIB" ]]; then
  echo "error: expected Android OEX executable not found: $LIB" >&2
  exit 3
fi

# Keep a stable CI artifact name alongside the exact OEX filename used by the manifest.
cp -f "$LIB" "$ROOT/$BUILD_DIR/chesszero"
echo "Android OEX engine: $LIB"
echo "Android OEX compatibility copy: $ROOT/$BUILD_DIR/chesszero"
