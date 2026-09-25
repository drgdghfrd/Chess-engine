# ChessZero v1.0.5 — Android performance / thread scaling

## Scope

v1.0.5 addresses a concrete performance problem found in the v1.0.4 root-level Lazy SMP implementation: every worker used a mutex-protected shared TT and a fresh `Search` object for each root iteration. On the measured host this made `Threads=2/4/8` slower than the single-thread path.

The change is deliberately search-safe rather than a blanket compiler-flag push:

- Search the first ordered root move synchronously to establish a strong alpha bound.
- Run the remaining root moves through a shared atomic work queue.
- The main search thread also participates in the queue.
- Each worker keeps a persistent private TT and private heuristic state for the duration of one search.
- The per-worker TT is fixed at 2 MB so the mobile memory footprint does not multiply the user-facing `Hash` setting.
- Workers use the current atomic root-alpha bound before starting the next root move.
- A root beta cutoff stops the remaining work.

## Reproducible host measurements

Workload: start position, HalfKP `ChessZero-v0.75-halfkp.nnue`, fixed depth, fresh process per run, three repetitions. These are host x86-64 measurements and are not Android device ratings.

### Depth 7

| Threads | Median nodes | Median wall (ms) | Median NPS | Observed best moves |
|---:|---:|---:|---:|---|
| 1 | 22,775 | 17.515 | 1,300,298 | b1a3 |
| 2 | 36,943 | 27.517 | 1,342,555 | b2b4; e2e3; e2e4 |
| 4 | 94,291 | 46.028 | 2,048,571 | a2a4; g2g3; h2h3 |
| 8 | 137,874 | 72.568 | 1,899,937 | b2b3; c2c3; e2e3 |

The important result is that the old implementation's 2/4/8-thread slowdown was removed at the NPS level on this workload: roughly 1.03x / 1.57x / 1.46x versus the 1-thread baseline. Wall time still grows with thread count at depth 7 because the independent root searches do more total work; this is expected for a simple root-parallel design.

### Depth 8

| Threads | Median nodes | Median wall (ms) | Median NPS |
|---:|---:|---:|---:|
| 1 | 58,862 | 66.044 | 891,251 |
| 2 | 55,614 | 61.439 | 905,191 |
| 4 | 38,144 | 40.916 | 932,260 |
| 8 | 220,493 | 120.753 | 1,825,978 |

Depth-8 measurements show that thread scaling is still workload-dependent. v1.0.5 therefore exposes thread control rather than hard-coding a universal Android optimum.

## Regression gates

- HalfKP incremental / refresh equivalence: PASS.
- HalfKP 40960x256 network inference: PASS.
- King-capture position regression: PASS.
- v1.0.3 bitboard cache regression: PASS.
- C API / cross-thread stop regression: PASS.
- v1.0.5 thread-scaling safety regression: PASS.

## Benchmark harness fix

`tools/threads_benchmark.py` now waits for UCI `bestmove` before sending `quit`. The previous version could measure process startup rather than the requested search because the engine is asynchronous.

## Android runtime guidance

The sample Android UI keeps a conservative default of 1 thread and 32 MB hash. The UI permits 1–8 threads. This is intentional until device-specific measurements are available.

For Android ARM64, the native build enables ARMv8 SIMD and keeps 16 KB ELF alignment explicit. Android's current guidance documents 16 KB page-size compatibility requirements and recommends current AGP/NDK combinations that support 16 KB packaging and alignment.

## Not yet enabled

PGO, `-ffast-math`, broad `+crypto`, and aggressive NEON intrinsics are not enabled as unconditional release defaults. They should be validated with search/PV/node-count regression and device-level thermal measurements before becoming defaults.
