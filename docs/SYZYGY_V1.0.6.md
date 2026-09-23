# ChessZero 1.0.6 — Syzygy backend

ChessZero 1.0.6 integrates the MIT-licensed jdart1/Fathom Syzygy probing code,
pinned to commit `c6cf6e8f2f4275e91e03c3612b8d17fe64253c5e`.

Fathom exposes WDL probing for search and root DTZ/WDL probing for selecting an
endgame move. ChessZero uses WDL during search and root probing for DTZ/root move
selection. It does not package the large `.rtbw/.rtbz` tablebase files in the
engine or APK; the user supplies `SyzygyPath`.

## Desktop build

```bash
cmake -S . -B build \
  -DCMAKE_BUILD_TYPE=Release \
  -DCHESSZERO_ENABLE_SYZYGY=ON
cmake --build build --target chesszero -j2
```

For an offline build, point CMake at a local Fathom checkout:

```bash
cmake -S . -B build \
  -DCHESSZERO_ENABLE_SYZYGY=ON \
  -DCHESSZERO_FATHOM_SOURCE_DIR=/path/to/Fathom
```

## Android

`android/CMakeLists.txt` enables the decoder by default. It uses the same pinned
repository/commit and accepts `CHESSZERO_FATHOM_SOURCE_DIR` for an offline
Android build.

## Runtime

```text
setoption name SyzygyPath value /path/to/tablebases
go depth 30
```

The search probes WDL only when the position is eligible and has no castling
rights or nonzero half-move clock. This avoids applying a WDL result that ignores
the 50-move clock. At the root, ChessZero first uses the DTZ/root API and falls
back to Fathom's WDL-only root ranking when `.rtbz` files are not available.
`SyzygyPath` accepts the platform path-list separator (`:` on Unix-like systems,
`;` on Windows).

## Verification note

The source architecture and API were verified locally with Fathom disabled. The
host used for this build could not resolve `github.com`, so the actual upstream
Fathom source could not be fetched and compiled in this environment. The pinned
source path is therefore deliberately exposed for an offline/local checkout and
for the eventual Android release build.
