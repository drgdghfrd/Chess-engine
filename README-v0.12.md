# UltraChess v0.12

Focus: real Android/OEX integration for Chessis.

The previous v0.11 source shipped the UCI engine and an OEX project prototype, but it did not yet contain a real ARM64 UltraChess executable. v0.12 adds a complete modern Android Studio OEX module around the existing UCI source, with `enginelist.xml`, provider, manifest registration, and an NDK build script.

Once `libultrachess.so` is built for arm64-v8a and placed in `android-oex/app/src/main/jniLibs/arm64-v8a/`, the resulting APK can be installed as an OEX engine package. A compatible Chessis release can then discover the package through the OEX provider mechanism.

No Stockfish binary is substituted for UltraChess.
