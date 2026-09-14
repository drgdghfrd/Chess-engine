# UltraChess v0.14

Independent search-strength upgrade informed by inspection of Stockfish source.

Changes from v0.12:
- Correct quiescence behavior when the side to move is in check: search all legal evasions instead of allowing an invalid stand-pat.
- Conservative null-move pruning with verification guard (`makeNull`/`undoNull`).
- Static-evaluation quiet-move pruning at shallow depths.
- Conservative SEE-inspired capture pruning.
- Keeps the existing NNUE, TT, aspiration, PVS/LMR, killer/history, book and OEX structure.
- No Stockfish executable or Stockfish source code is bundled into UltraChess.

All changes remain independent C++ implementation choices.
