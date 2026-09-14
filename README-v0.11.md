# UltraChess v0.11 — Arena / Engine-vs-Engine

v0.11 adds the practical **AI-vs-AI Arena** layer on top of v0.10.

## What works

- `tools/arena.py`: persistent UCI engine controller (one process per engine, reused for every move).
- `tools/arena.py --gui`: Tkinter desktop GUI with board, move log, Start/Stop, per-engine Book switches, depth/movetime and Threads.
- CLI match mode with W/D/L, result reason, move list and performance Elo estimate.
- Uses an UltraChess instance as validator to obtain legal-move/game status; therefore the match runner does not require `python-chess`.
- Adjudication: checkmate, stalemate, threefold repetition, 50-move rule, insufficient material, and maximum ply cap.
- UCI `d` now reports `status` and legal move count.
- Based on architectural lessons from the uploaded Stockfish source: persistent UCI processes, separate engine/validator responsibilities, thread-aware option handling, explicit time/depth controls, and a clean UCI boundary. UltraChess source is not a copy of Stockfish.

## Desktop GUI

```bash
python3 tools/arena.py --gui
```

Enter paths to two UCI engines. For example:

```text
Engine A: ./build-v10/ultrachess
Engine B: /path/to/stockfish
Validator: ./build-v10/ultrachess
```

Then choose depth or move time and press **Start Match**.

## CLI

```bash
python3 tools/arena.py \
  --engine-a ./build-v10/ultrachess \
  --engine-b /path/to/stockfish \
  --validator ./build-v10/ultrachess \
  --games 4 --depth 5 --threads 2 --book-a --book-b --alternate
```

No `python-chess` package is required.

## Android / Termux

The core engine already supports Android NDK/CMake. The Arena controller is Python, so on Android it is most straightforward to run the CLI in Termux. A native Android Arena UI (JNI + Kotlin/Java) is the next UI phase; this release does not pretend that a desktop Tkinter window is an Android APK.

## Stockfish package

The uploaded Stockfish tree was inspected for architecture/reference points (`thread.*`, `timeman.*`, `syzygy/tbprobe.*`, `nnue/*`, UCI and benchmark code). It remains a separate engine executable used as an external UCI opponent, not copied into UltraChess.
