# Benchmark research report: depth-5 reference run

This is an engineering validation record, not a published absolute Elo rating.

## Run conditions

- Arena: `tools/arena.py`
- Engine A: UltraChess built from the benchmark branch
- Engine B: Ubuntu package `stockfish 16-1build1`
- Validator: UltraChess
- Games: 24
- Search: depth 5
- Threads: 1
- Hash: 64 MB
- Opening suite: `tools/benchmark/openings_12x4.txt`
- Color schedule: 12 games with A White, 12 with A Black
- Opening book: disabled for both engines
- Per-game data: JSONL + PGN
- Binary hashes and UCI identities: recorded in JSONL

## Result

The post-legality-fix run recorded:

- W/D/L for Engine A: 22/1/1
- Score: 93.75%
- Performance Elo difference A-B: +470.4
- Approximate 95% Elo interval: +208.8 to +732.1
- Aborted games: 0
- Colors: 12 White / 12 Black

This result should not be treated as an absolute UltraChess Elo. It is a single-opponent, single-time-control, 24-game measurement at depth 5. A larger, multi-engine pool is required for a stable rating estimate.

## Important correction discovered during benchmarking

The Arena originally used an incorrect Elo conversion formula. It was corrected to use the score fraction:

`Elo = -400 * log10(1/p - 1)`

A regression test now locks this behavior down.

## Important legality correction

The engine's `Position::make()` previously allowed a move whose destination contained the opponent king. The search could therefore treat king capture as a legal move. The move maker now rejects:

- source/destination indices outside 0..63;
- source equal to destination;
- captures of a king.

A dedicated regression test covers king capture, and perft remains unchanged.

## Control run

A 12-game Stockfish-vs-Stockfish control was also run with the same Arena settings and UltraChess as validator:

- W/D/L: 6/2/4 for Engine A
- Performance Elo: +58.5
- Approximate 95% Elo interval: -131.3 to +248.2
- Colors: 6 White / 6 Black
- Aborted: 0

The control is consistent with a small-sample first-player/color effect and confirms that the Arena can record legal self-play games without producing the extreme one-sided score seen in the first UltraChess-vs-Stockfish run.

## Interpretation

The corrected 24-game UltraChess-vs-Stockfish result is evidence about this exact benchmark configuration. It is not enough to assign a general engine rating, and it should not be compared directly with public Elo lists that use different opponents, hardware, time controls, and statistical procedures.
