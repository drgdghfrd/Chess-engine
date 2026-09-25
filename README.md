# ChessZero v1.1.0 — Android/OEX release with runtime resource control


## v1.0.9 — Self-contained NNUE/OEX runtime

ChessZero v1.0.9 embeds the audited production network from `nets/ChessZero-v1.0.9-current.nnue` directly into the arm64 Android OEX executable and JNI library. Chessis/OEX launches no longer depend on filesystem access to the APK's private NNUE asset path. `EvalFile` accepts `embedded://ChessZero-v1.0.9-current.nnue`. The production `current.nnue` is the byte-identical `sf250k` baseline; the new 1M networks remain available for experiments until their training/data pipeline is corrected and strength-tested.


## 1M NNUE quick start

The bundle ships two experimental 1M networks:
- `nets/ChessZero-v1.0.9-single_1m.nnue` — CZNNUE32 Single-Layer.
- `nets/ChessZero-v1.0.9-multi_1m.nnue` — CZNNUE64 Multi-Layer `512→8→32→1`.

The production embedded default is `nets/ChessZero-v1.0.9-current.nnue`, byte-identical to the known Stockfish-derived `sf250k` baseline. During the final audit, `multi_1m` showed a saturated/constant output on a 1,000-record sample, so it is retained for retraining/benchmarking rather than silently promoted to the APK default.

Desktop/Termux:
```text
setoption name EvalFile value nets/ChessZero-v1.0.9-current.nnue
```

Android/Chessis uses the embedded network automatically; no external `.nnue` file is required at runtime.

## v1.0.7 — Syzygy backend + Android release hardening

v1.0.7 keeps the v1.0.6 engine architecture and hardens the production Syzygy/Fathom integration, deterministic Android toolchain, OEX packaging, and release validation surface. Historical thread-scaling details remain in `docs/ANDROID_PERFORMANCE_V1.0.5.md`.

Benchmark: `python3 tools/threads_benchmark.py --engine ./build/chesszero --network nets/ChessZero-v0.75-halfkp.nnue --depth 7 --threads 1,2,4,8 --repeats 3`.


## Network format

- HalfKP: `64 * 64 * 10 = 40960` inputs
- hidden layer: `256`
- first layer: signed int8 weights
- hidden bias: int16
- output weights: signed int8
- output bias: int32
- output shift: `8`
- file magic/version: `CZNNUE32 / 32`

`nets/ChessZero-v1.0.9-current.nnue` is the production HalfKP evaluation asset used by the embedded Android/OEX build. It is byte-identical to the known `sf250k` baseline; the 1M networks are retained as experimental assets pending a corrected training/data pipeline and playing-strength benchmark.

## Training tools

Requirements:

```bash
python3 -m pip install -r tools/requirements.txt
```

Convert FEN/target positions:

```bash
mkdir -p build/data
python3 tools/halfkp_dataset.py tools/data/sample_positions.txt build/data/sample.czds
```

Validate:

```bash
python3 tools/validate_dataset.py build/data/sample.czds
```

Train:

```bash
python3 tools/train_halfkp.py build/data/sample.czds build/ChessZero-v1.0.7-halfkp-trained.nnue --epochs 3 --seed 7501
```

The dataset target is from the side-to-move perspective. The historical v0.76 training pipeline uses a sparse-SGD trainer with a deterministic validation split.

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

The regression suite includes `v075_network`, which loads the packaged 40960x256 network and performs inference on representative FENs.


## Android app + OEX

ChessZero v1.0.7 now has two Android roles in one APK: a playable standalone UI with an 8×8 touch board and a standards-compatible Open Exchange (OEX) engine provider. Chess GUIs such as Chessis can discover the APK through the `intent.chess.provider.ENGINE` marker, `enginelist.xml`, and `ChessEngineProvider`.

The APK keeps the JNI runtime (`libchesszero_android.so`) separate from the OEX UCI executable (`libchesszero.so`). The latter is built as a PIE executable and exposed from APK assets for OEX clients.

Android release builds are pinned to SDK/target 35, NDK 29.0.14206865, and CMake 3.22.1. Fathom can use a preloaded local source via `CHESSZERO_FATHOM_SOURCE_DIR`; `CHESSZERO_FATHOM_OFFLINE=ON` makes a missing local checkout fail fast instead of fetching.

## Termux

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCHESSZERO_BUILD_TESTS=OFF
cmake --build build -j$(nproc)
printf 'uci\nisready\nposition startpos\ngo depth 5\nquit\n' | ./build/chesszero
```

## v0.76 real-data bootstrap

v0.76 adds a deterministic self-play dataset generator. It deliberately leaves
NNUE unloaded during data generation, so the existing classical evaluator is
the teacher. The positions come from legal self-play games, while the labels
are smooth classical scores. This is a bootstrap network, not an Elo claim.

Build the dataset tool:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j2 --target chesszero_selfplay
mkdir -p build/data
./build/chesszero_selfplay --games 20 --depth 3 --plies 80 \
  --random-opening-plies 6 --seed 7501
python3 tools/halfkp_dataset.py build/data/selfplay_teacher.txt build/data/selfplay_teacher.czds
python3 tools/validate_dataset.py build/data/selfplay_teacher.czds
```

Train:

```bash
python3 tools/train_halfkp.py build/data/selfplay_teacher.czds \
  build/ChessZero-v1.0.7-halfkp-trained.nnue --epochs 5 --seed 7501
```

Benchmark:

```bash
python3 tools/benchmark.py build/chesszero build/ChessZero-v1.0.7-halfkp-trained.nnue --depth 2
```


## v0.76 — strength measurement framework

This release adds a reusable match/measurement layer for comparing the existing HalfKP NNUE evaluation against the classical evaluation under the same search implementation.

### UCI evaluation mode

Use:

```text
setoption name EvalMode value nnue
setoption name EvalMode value classical
```

The default remains `nnue`. The search, move generation, hash and other engine components are unchanged; only the evaluation source is switched.

### Match tools

`tools/match/elo.py` computes score and Elo difference. `tools/match/run_match.py` provides a reproducible UCI match harness with alternating colors and JSON result logging.

The reported Elo is a **rating difference between the two configurations**, not an absolute ChessZero rating. For a production 1000-game run, keep hardware, hash, threads, opening set and time/depth conditions fixed and alternate colors.

The automated regression suite contains `v076_match_statistics`.


## v0.78 — Aspiration + PVS refinement

- Configurable root aspiration windows from depth 4+.
- Asymmetric geometric widening on fail-low/fail-high.
- PVS full-window re-searches are counted for tuning.
- UCI options: `Aspiration` and `AspirationWindow`.
- Search info reports aspiration and PVS re-search counters.

## v0.78 — LMR tuning

v0.78 tunes Late Move Reductions (LMR) while preserving the v0.77 aspiration/PVS framework.

UCI options:
- `LMR` — enable/disable late-move reductions (default `true`).
- `LMRAggression` — reduction aggressiveness from 50 to 200 (default `100`).

Search statistics exposed by the C++ API:
- `lmrSearches()` — reduced child searches.
- `lmrResearches()` — full-depth re-searches after a reduced search raises alpha.

The default setting is intentionally conservative. The tuning is designed to make late quiet-move reductions smoother and history-aware while protecting TT, killer, counter, checking and tactically relevant moves.


## v0.79 — Null Move + Razoring tuning

v0.79 adds conservative controls around the existing pruning logic from v0.78.

UCI options:
- `NullMove` — enable/disable null-move pruning (default `true`).
- `NullMoveAggression` — null-move eval/reduction aggressiveness, 50–200 (default `100`).
- `NullMoveVerification` — enable verification searches at depth 8+ (default `true`).
- `Razoring` — enable/disable near-horizon razoring (default `true`).
- `RazoringAggression` — qsearch razor margin, 50–200 (default `100`).

Search statistics exposed by the C++ API:
- `nullMoveSearches()` / `nullMoveCutoffs()` — null probes and successful cutoffs.
- `nullMoveVerifications()` / `nullMoveVerificationFailures()` — verification activity.
- `razorSearches()` / `razorFailLows()` — qsearch razor probes and safe fail-low prunes.

The default values preserve the v0.78 behavior at aggression 100. The tuning is intentionally incremental: null-move verification remains on by default and razoring only prunes after qsearch also fails low.

## v0.80 — History + Countermove tuning

v0.80 makes the existing history and countermove heuristics configurable so they can be tuned independently without changing the baseline behavior at 100.

UCI options:
- `History` — enable/disable quiet-move history and continuation-history updates (default `true`).
- `HistoryAggression` — scale history/continuation ordering and pruning from 50 to 200 (default `100`).
- `Countermove` — enable/disable exact countermove and contextual countermove history (default `true`).
- `CountermoveAggression` — scale countermove ordering/context weight from 50 to 200 (default `100`).

Search statistics exposed by the C++ API:
- `historyUpdates()` — successful quiet-move history/continuation updates.
- `countermoveHits()` — exact countermove ordering hits.

The default 100/100 configuration preserves the pre-v0.80 heuristic weights. The tuning layer is intentionally incremental so v0.78 LMR and v0.79 pruning can be measured without simultaneously rewriting the move-ordering architecture.

## ChessZero v0.81 — Search Heuristic Tuning

v0.81 begins the iterative tuning phase after LMR, Null Move/Razoring, History and Countermove.

### New controls
- `HistoryPruning`: enable/disable history-based late-move pruning.
- `HistoryPruningAggression`: 50–200, default 100.
- `FutilityAggression`: 50–200, default 100.

The default value 100 preserves the v0.80 baseline. These controls change shallow quiet-move pruning thresholds without changing evaluation or move legality.

### Telemetry
- `historyPrunes()` counts history-based late-move pruning events.
- `futilityPrunes()` counts shallow futility-pruning events.

## ChessZero v0.82 — Search Measurement Harness

v0.82 starts measurement-first iterative tuning. It does not declare a heuristic better or worse from a single run. Instead it provides a repeatable harness that runs the same positions in fresh engine processes/configurations and records completed depth, score, nodes, NPS and wall time.

### Measurement tool

`tools/measure_heuristics.py <engine> <network> --depth 8 [--csv result.csv]`

The default matrix compares the baseline with LMR, Null Move, History Pruning and Razoring individually disabled. Each configuration uses a fresh engine process to avoid transposition-table state leaking between configurations.

The C++ regression test `v082_measurement_harness` verifies that the same position remains legal/searchable under each configuration and prints node counts for observation.

## ChessZero v0.83 — Heuristic Tuning Matrix

v0.83 adds `tools/tuning_matrix.py`, a repeatable matrix runner for LMR, Null Move, Razoring, History, Countermove, History Pruning and Futility. Each configuration uses a fresh engine process and records depth, score, nodes, NPS and wall time. The matrix is an observation tool; it does not rank configurations or estimate Elo.

## ChessZero v0.84 — Automated Heuristic Analysis

v0.84 adds `tools/heuristic_report.py` to summarize tuning CSVs with median/mean nodes, median NPS, mean score, score spread and a descriptive stability flag. The stability threshold is configurable with `--max-score-drift` and is not a strength rating.

Example:
`python3 tools/heuristic_report.py tuning.csv --report tuning_report.txt --max-score-drift 100`

The report deliberately avoids selecting a best setting. Configuration changes should be validated on a larger position set and, later, match testing before becoming the new baseline.

## ChessZero v0.85 — Heuristic Stabilization / Phase 3 Exit

v0.85 freezes the Phase 3 search-heuristic baseline rather than introducing another
new pruning algorithm. The default tuning point remains 100 for all aggression
controls, with documented safety clamps at 50–200.

The stabilization regression suite checks:
- all v0.78–v0.84 heuristic defaults and aggression clamps;
- deterministic root move and score for a fixed-depth fresh search;
- legal root moves with the major search heuristics disabled together;
- non-zero completed search/node counts.

This is a regression/stability check, not an Elo claim. A new baseline should only
be promoted after larger match tests and repeated measurements on the target build.

### Phase 3 status

v0.77–v0.85 now form the search-tuning/stabilization phase. v0.85 deliberately
keeps the existing defaults unchanged so the next phase can measure multithreaded
changes against a stable single-thread baseline.


## v0.86 — Lazy SMP baseline

ChessZero v0.86 enables the existing root-parallel search as a conservative Lazy SMP baseline. `Threads` now accepts 1..64; `Threads 1` preserves the v0.85 single-thread path, while values above 1 distribute root moves across worker searches. Workers use private search state/TTs to avoid unsafe shared mutable heuristic state. This release focuses on correctness and stress-testability rather than claiming a speedup benchmark. UCI reports `Threads` max 64.


## v0.87 — Shared TT / Thread Coordination

v0.87 advances the v0.86 Lazy SMP baseline by sharing the main transposition table
with root workers. TT probes/stores use striped mutexes so concurrent workers do not
race on TT entries. Worker search state remains private; only the TT is shared.
This release prioritizes correctness and race safety over claiming a speedup.

New test: `v087_shared_tt`.


## v0.88 — Parallel Search Stabilization

v0.88 stabilizes the v0.86/v0.87 Lazy SMP path before further multithreading work.

- Root workers publish only fully completed root-move searches.
- Hard-stop/deadline checks are propagated through worker search state.
- Partial/aborted root searches are not promoted to the completed result set.
- The coordinator retains the previous legal root move if no worker completes before the hard stop.
- Added telemetry for aborted parallel roots and best-result updates.
- Added `tests/v088_parallel_stabilization_tests.cpp` and CTest coverage.

This release is a correctness/stability milestone; it does not claim a specific SMP speedup or Elo change.


## v0.90 — Multithreading Baseline

v0.90 closes Phase 4 with a regression baseline for the existing Lazy SMP and shared-TT implementation. `Threads` remains a UCI spin option from 1 to 64; `Threads 1` is the single-thread reference path, while values above 1 exercise root-parallel Lazy SMP with the shared transposition table and per-worker heuristic state.

The new `tests/v090_multithreading_tests.cpp` checks single-thread and multi-thread searches, root-move validity, completed depth, Lazy SMP activity, and thread-count clamping. `tools/threads_benchmark.py` provides a repeatable UCI measurement harness for nodes and wall-clock time across thread counts. The benchmark is intentionally descriptive: it does not claim Elo gain or a universal speedup.

Phase 4 status: Lazy SMP, shared TT, parallel stabilization, stop/race stress regression, and the multithreading baseline are now represented in the project test suite. The v0.89 stress regression is retained in the v0.90 package as `tests/v089_stress_tests.cpp`.


## v0.91 — Syzygy integration foundation

v0.91 starts the release-candidate tablebase work. The engine now validates `SyzygyPath`, clamps `SyzygyProbeLimit` to 0..7, counts pieces, and reports seven-piece eligibility through `d` and `tb status`. The UCI surface is ready for a future WDL/DTZ decoder, but v0.91 deliberately does not claim tablebase probing is available yet.

New regression: `v091_syzygy_integration`.

## v0.93 — Syzygy WDL/DTZ probe API

The tablebase layer now discovers `.rtbw` WDL and `.rtbz` DTZ assets and exposes
`probe()` plus the UCI command `tb probe`. The current build deliberately keeps
WDL/DTZ unavailable until a real compressed-table decoder is linked; filenames
alone never produce fabricated tablebase results.

## v0.93 — PolyGlot opening-book reader foundation

v0.93 adds a standards-compatible **PolyGlot binary record reader** for `.bin` books: 16-byte big-endian records, sorted-key lookup, move decoding, and weight-based selection. The existing text book format remains supported. The canonical 781-entry PolyGlot Zobrist hashing backend is intentionally kept separate; `probePolyglotKey()` accepts an already-computed canonical PolyGlot key so the binary reader can be tested without silently substituting ChessZero's internal Zobrist keys. UCI continues to use the existing text-book path until the canonical hashing backend is integrated.


## ChessZero v0.94 — PolyGlot canonical-hash integration gate

v0.94 separates the standard PolyGlot position-key interface from ChessZero's private Zobrist implementation. The binary book reader from v0.93 remains usable through explicit keys, while UCI book probing will not silently use incompatible keys. The canonical 781-entry PolyGlot random table is deliberately not claimed as complete in this gate; it must be vendored and verified against published PolyGlot test vectors before binary books are enabled automatically.


## v0.95 — Canonical PolyGlot hashing

- Vendored the complete 781-entry canonical PolyGlot random table.
- Implemented canonical PolyGlot hashing for pieces, castling, conditional en-passant file, and side-to-move.
- Enabled direct `.bin` PolyGlot book probing through the existing UCI BookFile/UseBook path.
- Added the standard starting-position vector `0x463B96181691FC9C` and an en-passant regression vector.
- The private ChessZero Zobrist key remains separate from the PolyGlot key.


## v0.96 — UCI PolyGlot book workflow hardening

v0.96 hardens the opening-book path around the canonical PolyGlot backend. `BookFile` now remembers the configured filename and supports paths containing spaces; `book reload` reloads that configured path instead of always falling back to `book.txt`; `book status` reports the active file and canonical-hash readiness; and the diagnostic commands `book key` and `book probe` expose the current PolyGlot key/probe without changing search behavior. The direct `.bin` reader and canonical 781-entry hash from v0.95 remain intact.

## v0.97 — UCI/RC hardening

- UCI search now runs asynchronously, so `stop` can interrupt an active search instead of waiting for the original `go` call to return.
- `quit`, `position`, `setoption`, `isready`, and `ucinewgame` synchronize with any active search before changing engine state.
- Search runs on a private position snapshot, avoiding races with the main UCI position while a worker is active.
- Added `ucinewgame` hash reset handling.
- `go infinite` is accepted and searches to the engine's maximum supported depth until `stop`/`quit` interrupts it.
- `ponder` is accepted as a compatible input token and treated as a normal search; full ponder-hit semantics remain outside this milestone.
- UCI identity is `ChessZero v0.99`; the asynchronous v0.97 lifecycle behavior remains covered by regression tests.


## v0.98 — UCI time-management hardening

v0.98 hardens clock-aware search allocation. The UCI surface now exposes `Move Overhead` (milliseconds reserved for GUI/OS/network latency) and `Slow Mover` (percentage scaling of the regular thinking budget). The existing `wtime/btime/winc/binc/movestogo` allocator now applies those controls while retaining a hard safety reserve. Search results expose `timebudget_soft` and `timebudget_hard` telemetry, making clock-budget behavior observable in GUI/protocol regression tests.

## v0.99 — Release Candidate hardening

v0.99 is the release-candidate stabilization pass before v1.0. The UCI layer now reports and ignores malformed/illegal moves received through `position ... moves` instead of passing a default move into the board state. The release also keeps the asynchronous v0.97 search lifecycle and v0.98 clock controls intact while adding a focused RC regression suite for repeated searches, `ucinewgame` reset behavior, immediate `stop`, malformed position input, and UCI protocol sequencing.

This milestone is a correctness/regression gate; it does not claim a new Elo result. The current release includes an optional Fathom-backed Syzygy WDL/DTZ probing path; tablebase files themselves are supplied by the user.


## ChessZero v1.0 — Stable Release

v1.0 freezes the v0.99 release-candidate behavior and adds the release surface needed for a stable source package. The main CMake target now has portable thread linkage through `Threads::Threads`, install rules, a version header, and platform documentation for Linux, Windows, and Android NDK/native-library builds.

### Release documentation

- `docs/UCI.md` — UCI protocol, options, lifecycle, book and tablebase diagnostics.
- `docs/BUILD.md` — Linux/macOS/Termux, Windows, Android NDK and installation commands.
- `docs/RELEASE.md` — platform scope and verification policy.
- `CHANGELOG.md` — v1.0 release notes and deliberate limitations.

### Release packaging

Use `python3 tools/release_package.py . ../ChessZero-v1.0.zip` from the project directory to create a clean source archive without transient `build/` contents.

### v1.0 claims

The release does not publish an absolute Elo number: no independently verified fixed-hardware/time-control match result is bundled with this source package. Syzygy probing is optional and requires user-supplied `.rtbw/.rtbz` files; the canonical PolyGlot hash and `.bin` reader are retained from v0.95–v0.96.

## v1.0.4 — Android runtime

This release adds the production-facing native boundary for an Android APK:

- opaque C API in `src/api/EngineAPI.*` (no C++ ABI dependency from Kotlin);
- JNI bridge in `android/app/src/main/cpp/jni_bridge.cpp`;
- minimal arm64-v8a Android app shell with background search and responsive stop;
- HalfKP network copied from APK assets to private app storage on first run;
- explicit 16 KB ELF alignment in the Android CMake path;
- native API regression covering FEN, network load, search, stop-from-another-thread and C ABI.

The release has been host-built and JNI-linked for interface validation. A final APK build requires
a local Android SDK/NDK/Gradle installation; the development container does not include that full toolchain.

## v1.0.2 — Benchmark preparation patch

This patch keeps the ChessZero v1.0 engine and HalfKP 40960x256 network intact while
hardening the cross-engine measurement layer. The UCI match runner now waits for an
actual `bestmove` line after arbitrary `info` output, applies benchmark defaults before
user-supplied UCI overrides, and keeps the king-capture position-state regression test.
It does not publish a new Elo result.

For a real ChessZero-vs-Stockfish measurement, keep the engine binary, network file,
threads, hash, search limit, opening set and hardware fixed; alternate colors and save
the complete game log plus measurement metadata.

## v1.0.1 — Cross-engine measurement tooling patch

The v1.0.1 patch fixes the match/measurement harness so it can compare **two
independent UCI executables**. `tools/match/run_match.py` now accepts
`--engine-a` and `--engine-b`, keeps per-engine modes/options/search limits,
and alternates colors each game.

Game legality and result adjudication are performed locally in the Python
harness, so the opponent does not need ChessZero-specific `d` or
`info string status=` output. Illegal engine moves are reported as protocol
errors rather than silently counted as losses/draws.

The saved match JSON records both executable paths and all engine-specific
settings. `summary.elo_delta` remains an **Elo difference under the measured
match conditions**, not an absolute engine rating. An optional `--rating-b`
records an explicit reference-rating assumption and derives a corresponding
A estimate without hiding the assumption.

This patch still does not publish a new ChessZero Elo result. A real rating
claim requires a documented independent opponent binary, fixed conditions,
color balancing, enough games for a useful confidence interval, and the full
match artifact.


## Benchmark status

See `docs/BENCHMARK_STATUS.md` for the reproducible ChessZero-vs-Stockfish measurement profile and the locally verified benchmark gates.


## v1.0.3 Android performance foundation

This tree adds incremental Position bitboards, per-ply quiescence move-buffer reuse, a single-thread TT fast path, optional LTO, explicit arm64-v8a NEON settings, and 16 KB ELF alignment support for older Android NDKs. See `docs/PERFORMANCE_ANDROID_V1.0.3.md`.

## Batch 2L — Android release hardening

The canonical workspace is organized under `chesszero/`. The pinned Fathom source contract is recorded in `third_party/Fathom/SOURCE.lock`; use `tools/vendor_fathom.sh` to populate the exact commit before an offline release build. Android OEX/APK validation is available under `tools/android/`.

## v1.1.0 runtime resource model

`Threads` is the number of active search workers. `Hash` is the total transposition-table memory budget for the whole search context, not `Hash × Threads`. Lazy-SMP contexts use private TT slices so they do not contend on a shared mutex in the search hot path; the slices are rebalanced when Threads or Hash changes. Changing Threads or Hash while the engine is idle reconfigures the worker pool/TT layout before the next search.

Battery-saving is controlled by the host GUI. ChessZero does not attempt to override a GUI time limit; it obeys the `go` command and its time budget.
