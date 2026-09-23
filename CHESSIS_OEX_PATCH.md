# Chessis/OEX Compatibility Patch

This patch keeps the standard Kalab OEX discovery contract and hardens the file-delivery path.

## What was intentionally NOT changed

- No extra manifest `<meta-data>` was added for `enginelist.xml`.
- `chess.provider.engine.authority` remains on the activity, as required by the historical OEX resolver.
- `res/xml/enginelist.xml` remains the fixed resource name used by the resolver.
- The OEX executable remains `libchesszero.so` and targets `arm64-v8a`.

## What was changed

1. `android/app/build.gradle.kts` marks `.so` assets as uncompressed so `AssetManager.openFd()` can return a direct file descriptor.
2. `ChessEngineProvider` keeps the fast `openFd()` path, adds a compressed-asset cache fallback, and also implements `openFile()` for consumers that use `openFileDescriptor()`.
3. The APK validator now rejects a compressed `assets/libchesszero.so` because that would violate the provider's zero-copy descriptor contract.
4. The Android/OEX static integration test now checks these compatibility guards.

## Phone test path

1. Push this source tree to a GitHub repository.
2. Open Actions → Android native → Run workflow; the workflow now supports manual phone-triggered runs.
3. Download the `chesszero-android-release` APK artifact to the phone.
4. Install the APK. Do not extract `libchesszero.so` manually; OEX discovery is provided by the installed APK/provider.
5. Open Chessis and use its OEX engine discovery/add-engine flow.
6. Select ChessZero and run an Engine-vs-Engine match against another installed engine.

If an older ChessZero OEX copy is already cached inside Chessis, update/reinstall the engine entry so the new APK's version is recopied.
