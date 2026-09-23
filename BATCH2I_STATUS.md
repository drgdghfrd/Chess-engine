# ChessZero v1.0.6 — Batch 2I status

## Scope

Batch 2I hardens the Syzygy integration with a repeatable 3–7 piece matrix and
regression coverage around the runtime conditions under which Fathom probing is
allowed.

## Added

- `tests/v107_syzygy_matrix_tests.cpp` (promoted from the historical v1.0.6 test) covers 3, 4, 5, 6, and 7-piece positions,
  plus an 8-piece negative control.
- Multi-directory `SyzygyPath` scanning is exercised through a nested fixture.
- WDL probing is checked for every eligible piece count when the Fathom backend is enabled.
- Non-zero halfmove clock and non-zero castling rights are checked as runtime probe guards.
- Root DTZ/result decoding is checked in the Fathom-compatible test build, including
  WDL class, DTZ class, DTZ plies, and decoded root move.
- The release metadata test was updated to the current `1.0.6` version.
- The v0.97 UCI protocol test now accepts the CMake-provided `CHESSZERO_BIN`, avoiding
  a hard-coded `build/chesszero` path.

## Verification

### Fathom disabled

- v0.91 Syzygy integration: PASS
- v0.92 Syzygy probe API: PASS
- v1.06 Syzygy 3–7 piece matrix: PASS
- v0.93 real-backend API test: SKIP/PASS as the expected no-Fathom guard
- v0.97 UCI hardening: PASS
- v0.97 UCI protocol: PASS with the CMake binary override
- v0.98 time management + protocol: PASS
- v0.99 engine + protocol: PASS
- v1.0 release metadata + protocol: PASS after version-sync fix
- v1.03 bitboard cache: PASS

### Fathom-compatible API build

A local stub that mirrors the upstream Fathom public header/API was used to compile
and run the Syzygy matrix and v0.93 backend smoke test. Both passed. This validates
that ChessZero's adapter uses the public API shape rather than a private/non-upstream
prototype.

The actual upstream Fathom source was still not fetched in this environment because
`github.com` is unreachable from the build container. No synthetic tablebase results
are packaged into ChessZero.
