## v1.0.9 (multi-layer NNUE) — HalfKAv2-style 512->8->32->1
- NNUE inference: new CZNNUE64 format with multi-layer forward pass (L1 concat stm-first -> L2=8 clipped -> L3=32 clipped -> scalar). Backward-compatible with CZNNUE32 single-layer; loadMemory auto-detects.
- Incremental accumulator update unchanged (addFeature/applyMoveFeatures/refresh).
- New trainer: tools/train_halfkp_ml.py (full backprop through L2/L3, weight decay, lr decay).
- UCI candidate list prefers ChessZero-v1.0.9-ml.nnue (CZNNUE64).
- Verified: perft(5)=4865609, both network formats load and search.

## v1.0.9 — Magic bitboards, bitboard SEE, x86 SIMD, Stockfish-distilled NNUE
- Performance: replaced naive slider loops with fancy magic bitboards (Berserk-standard constants); rook/bishop/queen attacks now O(1) lookup.
- Performance: SEE rewritten as bitboard Swap Algorithm (no Position copy, no movegen, no heap allocation).
- Performance: NNUE accumulator update gained AVX2 (16-wide) and SSE4.1 (8-wide) x86 vector paths; previously only NEON.
- Fathom updated c6cf6e8 -> c9c6fef (C++20 crash, alignment, tb_init/tb_free race fixes). SOURCE.lock refreshed.
- New default network: ChessZero-v1.0.9-sfdistill.nnue — HalfKP 40960x256 trained on 30k positions labeled by Stockfish 16 (depth 6, classical eval), replacing the v0.75 bootstrap self-play network. UCI candidate list prefers the new net.
- Polyglot Zobrist table verified canonical (781 entries, Random[0]=0x9D39247E33776D41) — unchanged.
- Verified: perft(5)=4865609, full test suite builds, startpos search returns sane main-line moves.

## v1.0.8 — Self-contained Android NNUE/OEX runtime

- Embedded the release HalfKP-40960x256 NNUE directly into the Android OEX executable and JNI engine.
- Removed the runtime dependency on app-private/assets `nets/*.nnue` paths for Chessis/OEX launches.
- Android UI now consumes the same embedded network instead of copying a 10 MiB asset at startup.
- Added `EvalFile` support for `embedded://ChessZero-v0.75-halfkp.nnue` and automatic embedded NNUE initialization.
- Added release regression coverage for the embedded-NNUE packaging contract.

## v1.0.7 — Release hardening

- Synchronized current engine and Android release metadata to v1.0.7 (versionCode 107).
- Replaced the custom Gradle bootstrap launchers with the standard Gradle 8.7 Wrapper scripts backed by the pinned official Wrapper JAR.
- Kept the vendored Fathom source pinned to commit c6cf6e8f2f4275e91e03c3612b8d17fe64253c5e and corrected the Syzygy matrix fixture so invalid placeholder files cannot masquerade as real tablebases.
- Removed the checked-in `build-linux/` build artifact from the source-release package.
- Added release validation for the arm64 Android/OEX packaging surface; physical Chessis/device execution remains an integration check when an Android device is connected.

## v1.0.6 — Batch 2M

- Pinned Android NDK to 29.0.14206865 while keeping compileSdk/targetSdk 35, CMake 3.22.1 and arm64-v8a.
- Hardened the Gradle 8.7 bootstrap launcher with pinned distribution SHA-256 verification; removed the stale wrapper-JAR checksum claim.
- Added deterministic Fathom source verification against `third_party/Fathom/SOURCE.lock`.
- Android CMake now fails closed by default when the vendored Fathom source tree is missing.
- Tightened 16 KB ELF/OEX validation and added host OEX alignment regression.

## v1.0.6 — Batch 2L — Android release hardening

- Organized the canonical development workspace under the `chesszero/` root.
- Added a pinned Fathom `SOURCE.lock` manifest and `vendor_fathom.sh` bootstrap script.
- Added explicit offline/required-vendored Fathom fail-fast behavior.
- Added Android APK/OEX 16 KB ELF validation tooling.
- Hardened the OEX provider to expose the engine read-only.
- Android OEX build helper now defaults to the vendored Fathom path and offline release mode.
- Real Android arm64 APK build remains unverified in this container because the required Android SDK/NDK/CMake toolchain is not installed.

## v1.0.6 — Batch 2K — Android playable UI + OEX

- Replaced the FEN/search-only Android screen with a playable board UI for human-vs-engine games.
- Added native move application, undo, new-game and current-FEN APIs.
- Restored OEX discovery metadata, `enginelist.xml`, and a read-only `ChessEngineProvider`.
- Added a separate Android PIE UCI executable for OEX; it is not the JNI shared library.
- Pinned Android SDK 35 / target 35, NDK 29.0.14206865 and CMake 3.22.1.
- Added explicit Fathom local-source and offline controls.
- Added Android/OEX integration regression tests.


## v1.0.6 — Batch 2I

- Added a repeatable Syzygy 3–7 piece eligibility/probe matrix with an 8-piece negative control.
- Added regression coverage for multi-directory `SyzygyPath`, halfmove-clock and castling guards, and root DTZ decoding.
- Synchronized the release metadata test with v1.0.6 and hardened the v0.97 protocol test to use the CMake-provided engine binary.
- Restored the Android `build_oex.sh` native build helper used by CI/OEX packaging.
- No search heuristic or default strength setting was changed in this batch.

## v1.0.6 — Batch 2H

- Added optional real Syzygy probing through pinned Fathom.
- Search uses WDL; root search uses DTZ/root and WDL-only fallback.
- Added multi-directory `SyzygyPath` handling and tablebase status diagnostics.
- Kept tablebase scores out of TT because the current TT key omits the half-move clock.
# ChessZero v1.0.6 — Syzygy backend + Android production

- Persistent private worker search contexts for Lazy SMP root parallelism.
- 2 MB per-worker TT to avoid multiplying the main Hash setting on mobile.
- Coordinator now participates in the root work queue.
- Fixed thread benchmark harness to wait for asynchronous `bestmove`.
- Added thread scaling measurements and regression coverage.

# Changelog

## v1.0.3 — Android performance foundation

- Incrementally cached all 12 piece/color bitboards in `Position`; hot `bitboards()` calls now return a const reference instead of rebuilding the board from 64 squares.
- Reused the search engine move buffer for quiescence search to remove a per-node dynamic allocation.
- Added a non-locking fast path to the transposition table for single-threaded searches; shared-TT workers enable striped locking explicitly.
- Added experimental Release LTO via `CHESSZERO_ENABLE_LTO`; it is OFF by default until optimizer-sensitivity checks are complete.
- Added explicit Android arm64-v8a NEON selection and 16 KB ELF alignment support for NDK r29 and lower.
- Preserved the existing search heuristics and UCI behavior; strength changes were not intentionally introduced.

# ChessZero Changelog

## v1.0.2 — benchmark preparation

- Hardened the cross-engine UCI match runner and added UCI identity capture.
- Added optional JSONL and PGN outputs for reproducible external matches.
- Kept the HalfKP 40960x256 engine/network path unchanged.
- Added benchmark status documentation and focused regression coverage.
- No new Elo claim is published by this patch.

## v1.0 — stable release

v1.0 freezes the v0.99 release-candidate behavior and packages the engine as a
stable source release.

### Release surface
- UCI identity is `ChessZero v1.0`.
- Portable CMake build for Linux, Windows, and Android NDK/native-library builds.
- CMake installation rules for the engine, bundled networks, README, changelog,
  and documentation.
- UCI documentation covers engine options, clock management, asynchronous
  `stop`, PolyGlot books, and Syzygy status/probe limitations.
- Release regression tests cover version metadata and the interactive UCI protocol.
- Source packaging helper excludes transient build artifacts from release archives.

### Deliberate limitations
- Syzygy WDL/DTZ compressed-table decoding is not included; the tablebase layer
  reports eligibility/file availability without fabricating WDL/DTZ results.
- PolyGlot uses the canonical 781-entry hash and binary reader; book files must be
  compatible with the implemented PolyGlot record format.
- The bundled HalfKP network is the existing v0.75 bootstrap network. This release
  does not claim a new independently measured Elo result.
- Android packaging in this source release produces a native engine/OEX-compatible
  shared library. A complete APK/UI application is outside the engine repository.

## v0.99 — release candidate

See the final v0.99 section in `README.md` for the RC hardening scope and regression
coverage carried into v1.0.

## v1.0.1 — Cross-engine measurement tooling patch

- Fixed `tools/match/run_match.py` so engine A and engine B can be different UCI executables.
- Added per-engine search limits and repeatable UCI options (`--option-a/--option-b`).
- Removed the dependency on ChessZero-specific `status` output for match adjudication.
- Added local legal-move, checkmate, stalemate, repetition, 50-move and insufficient-material adjudication.
- Illegal/malformed engine moves now fail the match as a protocol error instead of being scored as a chess result.
- Match JSON now preserves both executable paths and all measurement conditions.
- Added optional `--rating-b` reference-rating offset, explicitly labeled as an assumption.
- Added a regression test proving distinct executables are used and colors are alternated.

The engine remains identified as ChessZero v1.0; this is a tooling/reproducibility
patch and does not claim a new independently measured Elo result.

### Android packaging notes

- Added an explicit native Android build guide and a 16 KB ELF alignment checker.
- The release intentionally stops at the verified native engine layer; a complete APK/JNI shell is a subsequent integration milestone.

## 1.0.4 — Android Runtime & Mobile API

- Added a stable opaque C API in `src/api/EngineAPI.*` for Android/JNI integration.
- Added JNI bridge and a minimal arm64-v8a Android app shell.
- Search runs on a background executor in the sample app; `stop` remains responsive through the engine atomic stop flag.
- Packaged the HalfKP sample network as an Android asset and copy-on-first-run to app private storage.
- Added explicit 16 KB ELF linker alignment to the Android native build.
- Added v1.0.4 engine-API regression tests including cross-thread stop and C ABI smoke coverage.
- Kept default mobile profile conservative: 1 thread, 32 MB hash, arm64-v8a only.

## ChessZero 1.0.6 — Batch 2H

- Added real Syzygy probing through pinned Fathom.
- Added search-time WDL tablebase cutoffs for eligible positions.
- Added root DTZ/WDL tablebase move selection.
- Added real-backend smoke test and Syzygy diagnostics.
- Added reproducible Fathom pin and offline `CHESSZERO_FATHOM_SOURCE_DIR`.

## ChessZero 1.0.6 — Batch 2H
- Added a real Syzygy/Fathom integration path pinned to a reproducible upstream commit.
- Added WDL tablebase probing inside search for eligible positions.
- Added root DTZ/WDL probing and tablebase move selection.
- Added Syzygy runtime diagnostics and a backend API regression test.
- Added offline Fathom source override for desktop and Android builds.
