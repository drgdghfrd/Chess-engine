#!/data/data/com.termux/files/usr/bin/bash
set -e

ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

echo "[1/5] Checking tools..."
command -v cmake >/dev/null || { echo "Install cmake first: pkg install cmake"; exit 1; }
command -v clang++ >/dev/null || { echo "Install clang first: pkg install clang"; exit 1; }

echo "[2/5] Creating build directory..."
rm -rf build-termux
mkdir -p build-termux
cd build-termux

echo "[3/5] Configuring..."
cmake .. \
  -DCMAKE_BUILD_TYPE=Release \
  -DULTRACHESS_ANDROID_OEX=ON

echo "[4/5] Building..."
cmake --build . --target ultrachess -j"$(nproc)"

echo "[5/5] Copying..."
mkdir -p ../android-oex/app/src/main/jniLibs/arm64-v8a
cp ./libultrachess.so ../android-oex/app/src/main/jniLibs/arm64-v8a/libultrachess.so
chmod 755 ../android-oex/app/src/main/jniLibs/arm64-v8a/libultrachess.so

echo
echo "DONE:"
file ../android-oex/app/src/main/jniLibs/arm64-v8a/libultrachess.so
echo
echo "Dependencies:"
readelf -d ../android-oex/app/src/main/jniLibs/arm64-v8a/libultrachess.so || true
