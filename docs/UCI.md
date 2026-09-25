# ChessZero v1.0 UCI Guide

ChessZero speaks the Universal Chess Interface (UCI) on standard input/output.
The engine writes protocol responses to stdout and is intended to be driven by a
GUI or another UCI-compatible controller.

## Startup

The executable prints an informational banner and then waits for UCI commands.
A controller should normally send:

```text
uci
isready
```

ChessZero replies with `uciok` and `readyok` respectively.

## Position and search

Typical command sequence:

```text
position startpos moves e2e4 e7e5
go depth 12
```

Clock-aware search is supported with `wtime`, `btime`, `winc`, `binc` and optional
`movestogo`. `movetime` requests a fixed per-move budget. `go infinite` searches
until `stop` or `quit`.

A running search is asynchronous. `stop` interrupts it and the engine publishes a
legal `bestmove` after joining the search worker.

## Options

The main options exposed by `uci` are:

- `Threads`: 1–64 root-search workers.
- `Hash`: transposition-table size in MB, 1–2048.
- `Move Overhead`: reserved milliseconds for protocol/OS/transport latency.
- `Slow Mover`: scales the regular clock budget, 10–1000%.
- Search controls: `Aspiration`, `AspirationWindow`, `LMR`, `LMRAggression`,
  `NullMove`, `NullMoveAggression`, `NullMoveVerification`, `History`,
  `HistoryAggression`, `Countermove`, `CountermoveAggression`, `HistoryPruning`,
  `HistoryPruningAggression`, `FutilityAggression`, `Razoring`, and
  `RazoringAggression`.
- `EvalMode`: `nnue` or `classical`.
- `EvalFile`: path to a supported HalfKP network.
- `BookFile`: text or PolyGlot `.bin` opening book path.
- `UseBook`: enable/disable opening-book probes.
- `SyzygyPath`: tablebase directory.
- `SyzygyProbeLimit`: 0–7 piece eligibility limit.

## Diagnostics

ChessZero keeps a few useful engine-specific commands:

```text
book status
book reload
book key
book probe
tb status
tb probe
d
perft 4
eval
```

`book key` reports the canonical PolyGlot position key. `book probe` reports the
selected book move when one exists.

`tb probe` is an availability/status probe, not a claim of WDL/DTZ decoding in
v1.0. When no compressed-table backend is present, the WDL/DTZ result remains
reported as unknown.

## Lifecycle

When starting a new game, use:

```text
ucinewgame
position startpos
go depth 12
```

`ucinewgame` waits for a running search and clears the transposition table.
Commands that mutate engine state (`position`, `setoption`, `ucinewgame`) likewise
synchronize with an active search.

## Protocol notes

Malformed or illegal moves in `position ... moves` are reported with an `info string`
and ignored. They do not get passed into the board state as a default/zero move.

`ponder` is accepted as an input token for compatibility, but full ponder-hit state
management is not implemented in v1.0.
