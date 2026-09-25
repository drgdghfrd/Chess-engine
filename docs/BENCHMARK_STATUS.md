# ChessZero benchmark status — v1.0.2

## Verified locally

The v1.0.2 benchmark-preparation tree builds the main `chesszero` executable with GCC/CMake on Linux. The full CMake test graph is intentionally not used as a single blocking command in constrained CI environments; the focused benchmark/regression gates below have been run individually.

- UCI release protocol: PASS
- HalfKP incremental bit-exact suite: PASS
- HalfKP trained-network load/inference: PASS
- King-capture position regression: PASS
- Cross-engine match-runner regression: PASS
- 6-game ChessZero-vs-itself harness control at depth 3: 3W/0D/3L for engine A, score 50.0%

The self-match is only a harness/control check. It is not a strength estimate.

## Exact local artifacts

Record the SHA-256 of the exact executable and network used for every external match. The benchmark package intentionally does not bundle a Stockfish executable.

The intended external reference for the first real measurement is **Stockfish 19**, the current stable official release as of September 2026. Use one exact Linux x86-64 release binary and record its SHA-256 before the match.

## Match conditions

- ChessZero: `EvalMode=nnue`
- Network: `nets/ChessZero-v0.75-halfkp.nnue`
- Book: disabled
- Threads: 1
- Equal Hash: start with 64 MB
- Fixed search limit: same depth on both engines
- Colors: alternate every game
- Adjudication: local legal-move/checkmate/stalemate/repetition/50-move/insufficient-material logic
- Outputs: JSON, optional JSONL, optional PGN

A measured Elo difference is reported only for the selected match conditions. It is not an absolute ChessZero rating unless an external reference rating is deliberately supplied and labeled as an assumption.

## v1.0.4 mobile runtime

The Android layer now has a C ABI and JNI sample app. Native APK compilation is not claimed in
this container because a complete Android SDK/NDK/Gradle toolchain is not installed. Host-side
engine/API regression is run through CTest.
