# UltraChess v0.4

This release adds a compact 768->32->1 NNUE-style inference path and an offline trainer.

Important: no elite-strength trained network weights are shipped. The engine therefore uses its deterministic classical evaluator by default. A supplied `.nnue` model can be loaded after adding a UCI option in a later release.

The training format is documented in `tools/README.md`.
