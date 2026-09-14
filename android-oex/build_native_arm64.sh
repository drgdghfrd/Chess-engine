#!/usr/bin/env bash
set -euo pipefail

: "${ANDROID_NDK_ROOT:?Set ANDROID_NDK_ROOT to your Android NDK directory}"

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD="$ROOT/android-build-arm64"

rm -rf "$BUILD"

cmake -S "$ROOT" -B "$BUILD"   -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_ROOT/build/cmake/android.toolchain.cmake"   -DANDROID_ABI=arm64-v8a   -DANDROID_PLATFORM=android-24   -DCMAKE_ANDROID_STL_TYPE=c++_static   -DCMAKE_BUILD_TYPE=Release   -DULTRACHESS_ANDROID_OEX=ON   -DCMAKE_EXE_LINKER_FLAGS="-fuse-ld=lld -pie -static-libstdc++"

cmake --build "$BUILD" --target ultrachess -j"$(nproc 2>/dev/null || echo 2)"

OUT="$ROOT/android-oex/app/src/main/jniLibs/arm64-v8a"
mkdir -p "$OUT"

cp "$BUILD/libultrachess.so" "$OUT/libultrachess.so"
chmod 755 "$OUT/libultrachess.so"

echo
echo "=== UltraChess Android OEX binary ==="
file "$OUT/libultrachess.so"
echo
echo "=== Dynamic dependencies ==="
readelf -d "$OUT/libultrachess.so" 2>/dev/null || true
echo
echo "Created:"
echo "$OUT/libultrachess.so"
