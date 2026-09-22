# Reproducible engine benchmark

The Arena now treats engine A/B identity separately from board color, records aborted games, and can emit machine-readable JSONL plus optional standard PGN.

## Minimum protocol

Use a fixed engine binary/commit, fixed options, fixed hardware when possible, fixed time control, alternating colors, and a fixed opening set. Record every game's UCI move list and terminal reason.

Example command:

    python3 tools/arena.py \
      --engine-a ./ultrachess \
      --engine-b ./reference-engine \
      --games 100 \
      --movetime 100 \
      --alternate \
      --jsonl-out results/match.jsonl \
      --pgn-out results/match.pgn

The JSONL file contains match metadata, one record per game, and a summary. Aborted games are not included in W/D/L or Elo calculations.

--pgn-out requires the optional python-chess package because the Arena records UCI moves internally and converts them to SAN for PGN output.

## Interpretation

The printed performance Elo is an Elo difference against the specific opponent under the exact test conditions. It is not an absolute engine rating.

The tools/rating helper reports an approximate 95% interval using a Wilson interval applied to the half-point score fraction. For a publishable engine rating, use a multi-engine pool and a dedicated rating tool such as Ordo, rather than treating one head-to-head as an absolute Elo.

## Reproducibility checklist

Keep the exact engine commit/hash, compiler/toolchain, engine options, benchmark time control, number of games, color schedule, opening source/version, validator version, and generated JSONL/PGN together with the published result.


## Fixed opening suite

The repository includes `tools/benchmark/openings_12x4.txt`, a deterministic 12-line suite with four opening plies per line. The Arena cycles through the file when more games are requested. Because each line contains an even number of plies, the side to move remains White at game start and the `--alternate` flag cleanly swaps which engine receives White.

Example:

    python3 tools/arena.py \
      --engine-a ./ultrachess \
      --engine-b ./reference-engine \
      --games 120 \
      --depth 8 \
      --alternate \
      --openings tools/benchmark/openings_12x4.txt \
      --jsonl-out results/match.jsonl \
      --pgn-out results/match.pgn
