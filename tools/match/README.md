# Cross-engine match / Elo measurement

This directory provides the reusable strength-measurement layer for ChessZero.

- `elo.py`: score and Elo-difference calculations with a 95% interval.
- `run_match.py`: two-engine UCI match harness. **Each side has its own executable path.**

The runner performs chess legality and game-result adjudication locally, so the
opponent does not need ChessZero-specific commands such as `d` or
`info string status=`. Illegal moves are treated as a protocol error instead of
being silently converted into a loss/draw.

## Real cross-engine match

Example with ChessZero as A and a real Stockfish binary as B:

```bash
cd tools/match
python3 run_match.py \
  --engine-a ../../build/chesszero \
  --engine-b /path/to/stockfish \
  --depth 3 \
  --games 200 \
  --out cz_vs_sf.json
```

To give each engine different search limits:

```bash
python3 run_match.py \
  --engine-a ../../build/chesszero --engine-b /path/to/stockfish \
  --depth-a 6 --depth-b 3 --games 200
```

Extra engine-specific UCI options are repeatable:

```bash
python3 run_match.py \
  --engine-a ../../build/chesszero \
  --engine-b /path/to/stockfish \
  --depth-a 6 --depth-b 3 \
  --option-b "Skill Level=3" \
  --games 200
```

`Skill Level` is an engine configuration, not an independently measured absolute
Elo rating. Keep the exact settings in the saved JSON output and report them
alongside any result.

The `summary.elo_delta` value is the measured Elo difference between A and B
under the specified match conditions. It is **not** an absolute rating. You may
optionally pass `--rating-b` to offset the measured delta from a documented
reference rating for B; the output keeps that assumption explicit.

For publishable measurements, increase the game count until the 95% interval is
acceptably narrow and report the engine versions, binaries, search limits,
hardware, hash/thread settings, color balancing, and any opening suite used.

## v1.0.1 change

The old runner accepted one executable and compared ChessZero configurations of
that same process. The patched runner takes two paths and can therefore run a
real cross-engine match. It also adjudicates the board locally, avoiding hangs
with engines that do not implement ChessZero's diagnostic `d` command.
