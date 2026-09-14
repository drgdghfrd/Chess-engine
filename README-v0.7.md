# UltraChess v0.7

## New in v0.7
- NNUE v2 binary format with an actual incremental accumulator API.
- Search propagates NNUE accumulator state down the tree and updates only changed features for make/undo search nodes.
- UCI `EvalFile` option for loading a trained network.
- UCI `Threads` option retained as the public control; root-parallel search is staged for the next patch while preserving deterministic single-thread behavior by default.
- `ultrachess-selfplay` executable for generating FEN/evaluation datasets.
- Existing PVS/LMR/aspiration/TT/search features retained.

## Build
```bash
cmake -S . -B build-v07 -DCMAKE_BUILD_TYPE=Release
cmake --build build-v07 -j
```

## Test
```text
uci
isready
position startpos
perft 3
go depth 5
```
Expected Perft 3: **8902**.

## Load NNUE
```text
setoption name EvalFile value model.nnue
```
The model header must use magic `NUE4` and version `2` with the architecture defined in `NNUE.h`.
