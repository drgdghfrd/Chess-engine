#!/usr/bin/env python3
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
cmake = (ROOT / "android/CMakeLists.txt").read_text()
nnue_h = (ROOT / "src/nnue/NNUE.h").read_text()
nnue_cpp = (ROOT / "src/nnue/NNUE.cpp").read_text()
uci = (ROOT / "src/uci/UCI.cpp").read_text()
api = (ROOT / "src/api/EngineAPI.cpp").read_text()
activity = (ROOT / "android/app/src/main/java/com/chesszero/app/MainActivity.kt").read_text()
native = (ROOT / "android/app/src/main/java/com/chesszero/app/NativeChessZero.kt").read_text()
asset = ROOT / "android/app/src/main/assets/ChessZero-v0.75-halfkp.nnue"
network = ROOT / "nets/ChessZero-v0.75-halfkp.nnue"
checks = [
    (network.exists() and network.stat().st_size == 10486564, "release HalfKP network present and size locked"),
    (not asset.exists(), "Android APK no longer duplicates the NNUE asset"),
    ("bool loadMemory(const void* data" in nnue_h and "bool loadEmbedded()" in nnue_h, "NNUE memory/embedded load API declared"),
    ("loadMemory(" in nnue_cpp and "loadEmbedded()" in nnue_cpp, "NNUE embedded parser implemented"),
    ("_binary_embedded_nnue_bin_start" in nnue_cpp and "_binary_embedded_nnue_bin_end" in nnue_cpp, "ELF binary symbols consumed"),
    ("llvm-objcopy" in cmake and "elf64-littleaarch64" in cmake and "embedded_nnue.o" in cmake, "Android CMake embeds AArch64 NNUE object"),
    ('target_sources(chesszero_oex PRIVATE "${CHESSZERO_NNUE_OBJ}")' in cmake, "embedded object linked into OEX"),
    ('target_sources(chesszero_android PRIVATE "${CHESSZERO_NNUE_OBJ}")' in cmake, "embedded object linked into JNI"),
    ("CHESSZERO_EMBED_NNUE=1" in cmake, "embedded NNUE compile definition enabled"),
    ("uc::network().loadEmbedded();" in api, "JNI engine auto-loads embedded NNUE"),
    ("if (!network().loadEmbedded())" in uci, "UCI auto-loads embedded NNUE with file fallback"),
    ("embedded://ChessZero-v0.75-halfkp.nnue" in uci, "UCI advertises embedded EvalFile"),
    ('name=="NNUEEmbedded"' in uci and 'setEvalMode(EvalMode::Classical)' in uci, "NNUEEmbedded UCI switch is implemented"),
    ("nativeNnueLoaded" in native and "engine.nnueLoaded()" in activity, "Android UI reports embedded NNUE status"),
]
for ok, name in checks:
    print(("PASS " if ok else "FAIL ") + name)
if not all(ok for ok, _ in checks):
    raise SystemExit(1)
print(f"v1.10 NNUE/OEX packaging tests: PASS ({len(checks)} checks)")
