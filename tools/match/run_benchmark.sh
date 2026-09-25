#!/usr/bin/env bash
set -euo pipefail

CZ=${1:?usage: run_benchmark.sh /path/to/chesszero /path/to/stockfish [games] [depth] [hash_mb]}
SF=${2:?usage: run_benchmark.sh /path/to/chesszero /path/to/stockfish [games] [depth] [hash_mb]}
GAMES=${3:-100}
DEPTH=${4:-5}
HASH=${5:-64}

ROOT=$(cd "$(dirname "$0")/../.." && pwd)
OUT="$ROOT/build/match/chesszero_vs_stockfish_depth${DEPTH}.json"
mkdir -p "$(dirname "$OUT")"

python3 "$ROOT/tools/match/run_match.py" \
  --engine-a "$CZ" \
  --engine-b "$SF" \
  --mode-a nnue \
  --games "$GAMES" \
  --depth "$DEPTH" \
  --max-plies 200 \
  --option-a Threads=1 \
  --option-a Hash="$HASH" \
  --option-b Threads=1 \
  --option-b Hash="$HASH" \
  --out "$OUT"

echo "result=$OUT"
