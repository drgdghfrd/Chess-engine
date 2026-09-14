# NNUE training tools

`train_nnue.py` trains the v0.4 768->32->1 network from a CSV containing `fen,score`. Scores are centipawns from White perspective.

This is an educational/small NNUE pipeline. It is not comparable to modern Stockfish NNUE networks.

Example:
```bash
python3 train_nnue.py positions.csv model.nnue --epochs 10
```
