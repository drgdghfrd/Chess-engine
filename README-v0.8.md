# UltraChess v0.8

UltraChess v0.8 is a search-focused continuation of v0.7.

## New in v0.8
- Actual root-parallel search with configurable `Threads` (workers have independent TT/heuristics and share a stop/deadline signal).
- UCI time controls: `wtime`, `btime`, `winc`, `binc`, `movestogo`, plus `movetime` and `depth`.
- UCI `stop` support.
- Simple text opening book: `BookFile` + `UseBook`.
- TT hash now includes side, castling rights and en-passant square.
- Correct NNUE fallback: when no v2 NNUE file is loaded, the engine uses the classic material/bishop-pair evaluator instead of synthetic fallback weights.
- NNUE trainer output updated to binary version 2.
- Perft regression executable covering start position, Kiwipete, and a tactical endgame position.
- Tournament helper that reports W/D/L, performance Elo difference and an approximate 95% confidence interval when `python-chess` is installed.

## Build
```bash
cmake -S . -B build-v08 -DCMAKE_BUILD_TYPE=Release
cmake --build build-v08 -j
./build-v08/ultrachess-perft
```

Expected regression output ends with:
`all perft regressions passed`

## UCI examples
```text
uci
setoption name Threads value 4
setoption name BookFile value book.txt
setoption name UseBook value true
position startpos
isready
go wtime 300000 btime 300000 winc 2000 binc 2000 movestogo 30
```

## Important scope note
v0.8 does **not** claim 5000 Elo and does not bundle an elite trained NNUE. Syzygy probing is not included yet because no tablebase probing library is vendored into this release. The `Threads` option now performs real root-parallel work, but each worker intentionally keeps its own transposition/heuristic state for correctness and simplicity.
