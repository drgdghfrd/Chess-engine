# Batch 2M — Android release determinism / 16K / Fathom guard

## Completed

- Updated Android native toolchain pin to NDK `29.0.14206865`; CMake remains `3.22.1`, SDK/target remain `35`, ABI remains `arm64-v8a`.
- Kept explicit 16 KB linker alignment flags as an additional release guard.
- Reworked `gradlew` / `gradlew.bat` into a deterministic Gradle 8.7 distribution bootstrap: URL + SHA-256 are pinned in `gradle-wrapper.properties`, and the downloaded distribution is verified before execution.
- Removed the stale `gradle-wrapper.jar.sha256` manifest; the repository does not claim to ship `GradleWrapperMain` or a standard wrapper JAR.
- Added deterministic `tools/verify_fathom_vendor.sh` and made `tools/vendor_fathom.sh` verify the populated source tree against `SOURCE.lock`.
- Android CMake defaults to requiring the vendored Fathom tree; a missing source fails closed instead of silently FetchContent-cloning.
- Fixed 16 KB ELF validator wording and alignment handling to require at least `0x4000` on every `PT_LOAD`.

## Verification

- `v1.07 Gradle bootstrap tests`: PASS
- `v1.08 Android/OEX integration tests`: PASS (15/15)
- `v1.09 release packaging tests`: PASS
- Desktop Release build with Syzygy OFF: PASS
- Desktop UCI smoke: PASS
- Android-CMake host OEX build with API-compatible Fathom stub: PASS
- Host OEX ELF: ET_DYN/PIE; all PT_LOAD alignments `0x4000`
- CMake fail-closed guard when Fathom is missing: PASS

## Environment limitation

The runtime still cannot resolve GitHub, so the actual upstream Fathom source payload could not be transferred into `third_party/Fathom/src/` here. The project therefore does **not** claim a physically vendored Fathom tree or a real arm64-v8a APK build in this environment. The pinned source contract and verification tooling are ready for a networked/SDK-equipped release machine.
