#!/usr/bin/env python3
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
manifest = (ROOT / "android/app/src/main/AndroidManifest.xml").read_text()
engines = (ROOT / "android/app/src/main/res/xml/enginelist.xml").read_text()
provider = (ROOT / "android/app/src/main/java/com/chesszero/app/ChessEngineProvider.kt").read_text()
cmake = (ROOT / "android/CMakeLists.txt").read_text()
gradle = (ROOT / "android/app/build.gradle.kts").read_text()
activity = (ROOT / "android/app/src/main/java/com/chesszero/app/MainActivity.kt").read_text()
workflow = (ROOT / ".github/workflows/android-native.yml").read_text()

checks = [
    ("OEX action", 'intent.chess.provider.ENGINE' in manifest),
    ("OEX authority metadata", 'chess.provider.engine.authority' in manifest),
    ("OEX provider", 'ChessEngineProvider' in manifest and 'com.chesszero.app.engine' in manifest),
    ("OEX enginelist", 'libchesszero.so' in engines and 'arm64-v8a' in engines),
    ("OEX provider file", 'openAssetFile' in provider and 'libchesszero.so' in provider),
    ("OEX openFile compatibility", 'override fun openFile' in provider and 'ParcelFileDescriptor.dup' in provider),
    ("OEX asset is uncompressed", 'androidResources' in gradle and 'noCompress += "so"' in gradle),
    ("OEX executable target", 'add_executable(chesszero_oex' in cmake),
    ("Embedded NNUE generator", 'llvm-objcopy' in cmake and 'embedded_nnue.bin' in cmake and 'elf64-littleaarch64' in cmake),
    ("Embedded NNUE linked into OEX", 'target_sources(chesszero_oex PRIVATE "${CHESSZERO_NNUE_OBJ}")' in cmake and 'CHESSZERO_EMBED_NNUE=1' in cmake),
    ("Embedded NNUE linked into JNI", 'target_sources(chesszero_android PRIVATE "${CHESSZERO_NNUE_OBJ}")' in cmake and 'CHESSZERO_EMBED_NNUE=1' in cmake),
    ("No duplicate NNUE asset", 'assets/ChessZero-v0.75-halfkp.nnue' not in activity and not (ROOT / "android/app/src/main/assets/ChessZero-v0.75-halfkp.nnue").exists()),
    ("OEX PIE", '-fPIE' in cmake and '-pie' in cmake),
    ("OEX generated asset", 'generated/oex' in cmake and 'generated/oex' in gradle),
    ("JNI target remains separate", 'add_library(chesszero_android SHARED' in cmake),
    ("SDK 35", 'compileSdk = 35' in gradle and 'targetSdk = 35' in gradle),
    ("NDK pinned", '29.0.14206865' in gradle),
    ("CMake pinned", 'version = "3.22.1"' in gradle),
    ("Phone-triggerable GitHub Actions", 'workflow_dispatch:' in workflow),
    ("board UI", 'ChessBoardView' in activity and 'playHumanMove' in activity),
    ("game controls", 'New game' in activity and 'Undo turn' in activity),
    ("engine settings retained", 'Hash MB' in activity and 'Move time (ms)' in activity),
]
for name, ok in checks:
    if not ok:
        raise SystemExit(f"FAIL: {name}")

print(f"v1.08 Android/OEX integration tests: PASS ({len(checks)} checks)")
