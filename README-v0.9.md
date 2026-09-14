# UltraChess v0.9

v0.9 is a correctness/performance-focused continuation of UltraChess v0.8.

## New in v0.9
- Bitboard-driven pseudo move generation for pawns, knights, bishops, rooks, queens and kings.
- Bitboard attack detection, including pawn/knight/king and sliding attacks.
- Additional FEN validation for en-passant squares.
- Syzygy configuration/status adapter with piece-count eligibility checks. v0.9 does **not** claim to probe Syzygy files; a real probing backend must be linked in.
- `SyzygyPath` and `SyzygyProbeLimit` UCI options.
- `d` now prints tablebase status metadata.
- Standardized native benchmark executable covering start position, Kiwipete and a tactical middlegame.
- Added Kiwipete depth-3 Perft regression (97862).

## Build
```bash
cmake -S . -B build-v09 -DCMAKE_BUILD_TYPE=Release
cmake --build build-v09 -j
./build-v09/ultrachess-perft
./build-v09/ultrachess-bench
```

Expected Perft regressions:
- Start position depth 4 = 197281
- Endgame depth 4 = 43238
- Kiwipete depth 3 = 97862

## UCI tablebase configuration
```text
setoption name SyzygyPath value /path/to/syzygy
setoption name SyzygyProbeLimit value 5
d
```
The status line distinguishes path configuration from actual probing availability. In this release, `probing=false` by design unless a future build supplies a genuine Syzygy backend.

## Scope
v0.9 does not claim 5000 Elo. Strength must be measured by repeatable engine-vs-engine or external rating benchmarks. The move generator is now bitboard-driven, but legality still uses make/undo filtering; a later release can replace that with fully legal bitboard generation/pinning logic.


## Book control commands

Interactive console commands:

```text
book on       # enable opening book
book off      # disable opening book
book status   # show enabled/loaded state
book reload   # reload book.txt
```

The equivalent UCI option is:

```text
setoption name UseBook value true
setoption name UseBook value false
```

When Book is disabled, `go` always proceeds directly to the search.
