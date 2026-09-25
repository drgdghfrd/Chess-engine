# ChessZero v1.0.6 — Batch 2L Status

## Goal

Make the Android release path deterministic and prepare a self-contained source layout under `chesszero/`.

## Completed in this environment

- Consolidated the active ChessZero workspace and historical batch notes into the source-release tree.
- Added `third_party/Fathom/SOURCE.lock` with the pinned commit and upstream Git blob identifiers/sizes for the required Fathom files.
- Added `tools/vendor_fathom.sh` to populate the exact pinned Fathom source tree from upstream.
- Added explicit APK/OEX 16 KB ELF validation tooling.
- Hardened `ChessEngineProvider` to expose the OEX engine read-only.
- Updated the Android OEX build helper to default to the vendored Fathom path and fail offline when it is absent.

## Verification

- Existing Batch 2K host tests remain the baseline.
- Script syntax checks: PASS.
- OEX manifest/provider/static integration remains PASS from Batch 2K.

## External limitation

The current execution environment cannot resolve `github.com`/`raw.githubusercontent.com`, so the 70,630-byte upstream `src/tbprobe.c` payload and the other Fathom source payloads could not be transferred into the local filesystem through the available runtime. Therefore this environment does **not** claim a fully vendored Fathom tree or a real Android arm64 APK build.

The repository now fails closed for offline release builds: provide the verified pinned source tree under `third_party/Fathom/` (or set `CHESSZERO_FATHOM_SOURCE_DIR`) before using `CHESSZERO_FATHOM_OFFLINE=ON`.
