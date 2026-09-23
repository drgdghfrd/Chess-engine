# ChessZero v1.0.3 — Android performance foundation

## Scope

This release applies low-risk structural optimizations first. The goal is to reduce CPU time per searched node on ARM64 without deliberately changing the search heuristics, evaluation weights, or UCI protocol.

## Changes

### 1. Incremental Bitboards

`Position::bitboards()` previously rebuilt all 12 piece/color bitboards by scanning all 64 squares. v1.0.3 stores those bitboards in `Position` and updates only the squares touched by `make()`/`undo()`. `attacked()`, evaluation helpers, NNUE refresh, and null-move guards can therefore reuse the cached representation.

### 2. Quiescence move-buffer reuse

The quiescence search no longer creates a fresh `std::vector<Move>` at every node. It reuses the existing per-ply search buffer, avoiding allocator traffic in a very hot path.

### 3. Single-thread TT fast path

The private transposition table used by a single-threaded search no longer takes a mutex for every probe/store. Shared-TT worker searches explicitly enable the existing striped locking path. This optimization changes synchronization overhead only; it does not alter TT replacement policy or bounds.

### 4. Android build hardening

LTO is available as an explicit opt-in, but is OFF by default in this release because a preliminary host experiment changed search-node/PV behavior; that is treated as an optimizer-sensitivity investigation rather than a safe default. Android arm64-v8a explicitly enables ARMv8-A SIMD/NEON. The OEX link path also supports explicit 16 KB ELF alignment; this is required with NDK r27 and lower, while NDK r28+ enables 16 KB alignment by default.

## Host measurement

A repeatable start-position fixed-depth test was run seven times per build on the same x86-64 host. At depth 7, baseline v1.0.2 and v1.0.3 searched the same **34,545 nodes** and returned the same score/PV on every run. Median wall-clock measurements were:

| Version | Depth | Nodes | Median time | Median NPS |
|---|---:|---:|---:|---:|
| v1.0.2 baseline | 7 | 34,545 | 1.639 s | ~21.1k |
| v1.0.3 optimized | 7 | 34,545 | 0.961 s | ~36.0k |

The measured host-side wall time was about **41.4% lower** for this workload. This is a host microbenchmark, not an Android-device claim.

## Correctness gates

The following focused tests passed after the changes:

- `v041_simd`
- `v069_nnue_bitexact`
- `v072_halfkp`
- `v075_network`
- `v102_position_king_capture`

Perft spot-checks also preserved expected counts for the standard start position through depth 4 and the standard Kiwipete position at depth 3.

## Next optimization tier

The next gains should come from measuring, not guessing: move-generation profiling, qsearch/SEE cost, TT hit/lock contention under multi-threading, then PGO on representative Android positions. Search-strength tuning should be separated from pure speed work so Elo changes can be attributed to the intended search modification.


### LTO status

LTO is deliberately opt-in in v1.0.3. A preliminary host LTO experiment compiled successfully but changed the observed depth-7 search result (score/node count/PV) relative to the non-LTO build. That behavior is not accepted as a production optimization yet; it is an optimizer-sensitivity investigation for a later release.
