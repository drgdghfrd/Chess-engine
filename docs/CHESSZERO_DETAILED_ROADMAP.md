# ChessZero — Detailed Upgrade Roadmap

## Goal

Move from the proven v1.0.9 Chessis/OEX baseline to a stronger and more efficient
v1.1.0, then continue toward the planned 1.1.5 and 1.20.0 releases without
mixing strength changes until each runtime change has a clean benchmark.

## Phase 0 — Baseline lock (1–2 days)

1. Freeze the current working APK as `1.0.9-working`.
2. Record the device profile: Android version, ABI, CPU cores exposed to Chessis,
   RAM, battery-saver state, thermal state and Chessis version.
3. Record UCI settings for every benchmark.
4. Run the same opening suite with 1, 2, 4, 5 and 7 Threads and Hash 16, 32, 64,
   128 and 256 MB.
5. Save PGNs, NPS, nodes, depth and result logs as the baseline.

## Phase 1 — Runtime resources (week 1)

### A. Hash profiles

Support and test the low-memory profiles explicitly:

| Threads | 256 MB | 128 MB | 64 MB | 32 MB | 16 MB |
|---:|---:|---:|---:|---:|---:|
| 1 | 256 | 128 | 64 | 32 | 16 |
| 5 | 256 | 128 | 64 | 32 | 16 |
| 7 | 256 | 128 | 64 | 32 | 16 |

`Hash` means the **total TT memory budget**, not `Hash × Threads`.

For the current low-contention private-TT layout, the common 7-thread splits are:

- 64 MB → 16 + 8 + 8 + 8 + 8 + 8 + 8 MB
- 32 MB → 8 + 4 + 4 + 4 + 4 + 4 + 4 MB
- 16 MB → 4 + 2 + 2 + 2 + 2 + 2 + 2 MB

These are implementation details; the user-facing Hash value remains the total.

### B. Threads lifecycle

Verify:

- 7 → 5 → 1 switches fully rebuild the worker pool while idle.
- No worker from the old configuration survives into the next search.
- Search stop/restart does not leak threads.
- `isready` reports the applied runtime configuration.

### C. Battery Saver contract

Do not invent a new UCI Battery Saver option. Chessis owns that policy. The
engine must obey the host's `go` time limit and must never intentionally extend
a GUI-imposed deadline.

## Phase 1 exit gate (end of week 1)

- All low-hash profile tests pass.
- 1/2/4/5/7-thread smoke tests pass.
- No thread leaks across repeated reconfiguration.
- Hash totals never exceed the configured budget.
- UCI reports applied Threads/Hash correctly.

## Phase 2 — Search efficiency (week 2)

1. Staged move picker.
2. Reduce full-list sorting at every node.
3. Search-stack state instead of repeated heap/vector state.
4. IID and tactical extension audit.
5. Re-run the complete Threads × Hash matrix.

**Gate:** speed improvements must not introduce tactical regressions on the
existing tactical test suite.

## Phase 3 — NNUE/mobile performance (weeks 3–4)

1. Establish NNUE inference baseline on ARM64.
2. Measure SIMD/scalar path separately.
3. Profile accumulator refresh costs.
4. Evaluate lightweight network candidates.
5. Only promote a new network after validation and engine-match testing.
6. Measure CPU time, NPS, depth and battery/thermal behavior where possible.

## Phase 4 — Strength features (weeks 5–6)

1. Opening-book quality audit.
2. Middlegame/endgame evaluation audit.
3. Syzygy usage audit.
4. Tactical regression suite expansion.
5. Candidate search-parameter tuning using fixed match conditions.
6. Self-play dataset generation and NNUE training pipeline.

The self-learning loop updates data/network candidates; it must not silently
rewrite search code inside the APK.

## Phase 5 — Release hardening (weeks 7–8)

1. Freeze search parameters.
2. Freeze network candidate.
3. Run full Android/OEX packaging checks.
4. Test Chessis discovery and Engine-vs-Engine.
5. Test 16/32/64/128/256 MB Hash.
6. Test 1/2/4/5/7 Threads.
7. Test battery saver ON/OFF through the host's actual time controls.
8. Produce release notes, reproducibility manifest and benchmark bundle.
9. Build signed release APK.
10. Tag the release.

## Benchmark protocol

Use the same:

- device and Android build
- Chessis version
- engine build
- NNUE file
- opening suite
- Threads/Hash settings per experiment
- time control
- Syzygy/Book configuration

For strength comparisons use many games and report the number of wins, draws,
losses and score. Do not infer Elo from a handful of games.

## Milestones

- **M1:** v1.1.0 runtime resources stable.
- **M2:** v1.1.0 search core benchmark complete.
- **M3:** v1.1.5 NNUE/performance candidate selected.
- **M4:** 1.20.0 feature freeze.
- **M5:** 1.20.0 release APK passes Chessis Engine-vs-Engine test.
