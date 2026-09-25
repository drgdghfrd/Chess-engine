# ChessZero vs Stockfish measurement profile

This profile is a neutral, reproducible measurement recipe. It records the measured
result without treating the result as an absolute ChessZero rating unless an external
reference rating is explicitly supplied.

## Fixed conditions

- ChessZero executable: record SHA-256 of the exact binary used.
- ChessZero network: `nets/ChessZero-v0.75-halfkp.nnue`; record SHA-256.
- Stockfish executable: record exact release/build and SHA-256.
- ChessZero `EvalMode`: `nnue`.
- Opening book: disabled for both engines.
- Threads: `1` for both engines unless a separate SMP experiment is being measured.
- Hash: equal MB for both engines.
- Search limit: use one fixed depth or one fixed per-move time for both engines.
- Colors: alternate every game.
- Save the complete JSON result and the exact command line.

## Example depth-match command

```text
python3 tools/match/run_match.py \
  --engine-a ./build/chesszero \
  --engine-b /path/to/stockfish \
  --mode-a nnue \
  --games 100 \
  --depth 5 \
  --max-plies 200 \
  --option-a Threads=1 \
  --option-a Hash=64 \
  --option-b Threads=1 \
  --option-b Hash=64 \
  --out build/match/chesszero_vs_stockfish_depth5.json
```

The harness performs local legality/result adjudication. It waits for an actual
`bestmove` line and ignores preceding `info` output, which is required for normal
UCI engines.

## Result interpretation

`summary.elo_delta` is a performance difference under the measured conditions. It is
not an absolute ChessZero Elo. A confidence interval should be reported with the game
count, W/D/L, score, hardware, engine binaries, network hash, search limit and opening
set. A small game count is a smoke test, not a stable strength estimate.
