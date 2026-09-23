# ChessZero v1.0.6 — Batch 2H Syzygy backend

## Implemented

- Added a real Fathom adapter behind `CHESSZERO_ENABLE_SYZYGY` (default OFF for desktop, ON for Android).
- Search uses Fathom WDL for eligible tablebase positions.
- Root search uses DTZ/root probing and falls back to WDL-only root move ranking when `.rtbz` files are unavailable.
- `SyzygyPath` accepts the platform path-list syntax and status now scans all listed directories.
- Piece-count checks use a single occupancy popcount rather than scanning all 64 squares at every eligible probe.
- Tablebase results are deliberately **not stored in the existing TT** because the current Zobrist key does not encode the half-move clock.
- Worker searches inherit the Syzygy backend pointer for thread-safe WDL probing; root probing is performed only on the main search thread.
- `tb status` and `tb probe` expose largest-piece coverage, WDL/DTZ availability, and root move.

## Verification

Desktop with `CHESSZERO_ENABLE_SYZYGY=OFF`:

- v0.91 Syzygy integration: PASS
- v0.92 Syzygy probe API: PASS
- v0.93 real backend API test: SKIP (expected without Fathom)
- v0.54 iterative-deepening/time-budget: PASS
- v0.98 time-management: PASS

The Fathom-enabled CMake path was compiled against an API-compatible local stub;
`v0.93` passed and runtime smoke verified `SyzygyPath` path lists, `tb status`,
`tb probe`, and root-search move propagation.

The actual upstream Fathom checkout could not be fetched in this environment
because the host cannot resolve `github.com`. The project therefore pins the
upstream repository/commit and supports an offline/local `CHESSZERO_FATHOM_SOURCE_DIR`.
No fake tablebase results are included in the release.
