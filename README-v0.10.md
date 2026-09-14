# UltraChess v0.10

v0.10 focuses on practical operation and a substantially larger opening book.

## Opening Book

- `book.txt` contains **758 generated position/move entries** from 95 curated opening lines.
- Covers dozens of opening families, including Ruy Lopez, Italian, Scotch, King's Gambit, Four Knights, Vienna, Ponziani, Sicilian (Najdorf/Dragon/Classical/Scheveningen/Sveshnikov/Accelerated Dragon), French, Caro-Kann, Scandinavian, Pirc, Alekhine, Modern, Queen's Gambit, QGD, Slav, Semi-Slav, Nimzo-Indian, Queen's Indian, King's Indian, Grünfeld, Benoni, Benko, Dutch, English, Réti, Catalan, London, Colle, Trompowsky, Torre, Veresov, Blackmar-Diemer, Danish, Center Game, Philidor, Owen, Nimzowitsch, Polish and Bird.
- Multiple continuations are stored for positions shared by different lines.
- Selection is deterministic per position but can choose among available continuations instead of always taking the first line.

## Book controls

UCI options:

```text
setoption name UseBook value true
setoption name UseBook value false
setoption name BookFile value book.txt
```

Convenience commands:

```text
book on
book off
book status
book reload
book help
```

`go` checks the book first when `UseBook=true`. If no book move exists, normal search is used.

Example:

```text
uci
isready
book status
position startpos
go depth 16
book off
go depth 16
book on
```

## Build

```bash
cmake -S . -B build-v10 -DCMAKE_BUILD_TYPE=Release
cmake --build build-v10 -j2
```

Executables include `ultrachess`, `ultrachess-perft`, `ultrachess-bench`, `ultrachess-selfplay`, and `ultrachess-bookgen`.

## Important

The opening book improves opening move selection; it does not by itself increase engine Elo to a fixed number. Strength must be measured with repeatable engine-vs-engine or human benchmarks.
