# ChessZero v1.0.9 — NNUE 1M bundle

This bundle includes the ChessZero v1.0.9 source plus the newly trained 1M-position networks.

## Networks
- `nets/ChessZero-v1.0.9-single_1m.nnue` — CZNNUE32 Single-Layer 40960x256
- `nets/ChessZero-v1.0.9-multi_1m.nnue` — CZNNUE64 Multi-Layer 512->8->32->1 (experimental; not the production default yet)
- `nets/ChessZero-v1.0.9-current.nnue` — byte-identical copy of the known Stockfish-derived `sf250k` baseline, selected for production embedding after the 1M model integrity audit
- Android raw `.nnue` asset: intentionally omitted. `android/CMakeLists.txt` embeds `nets/ChessZero-v1.0.9-current.nnue` directly into both the OEX and JNI native targets, preventing the previous OEX asset-path problem and avoiding a duplicate ~10 MB asset.

## UCI
For desktop/Termux production/default:
`setoption name EvalFile value nets/ChessZero-v1.0.9-current.nnue`

For 1M Multi-Layer experiments:
`setoption name EvalFile value nets/ChessZero-v1.0.9-multi_1m.nnue`

For Single-Layer comparison:
`setoption name EvalFile value nets/ChessZero-v1.0.9-single_1m.nnue`

The Android copy is intended to be embedded by the existing CMake/OEX build; it is not an external file dependency at runtime.

## Integrity

Run `python3 tests/v111_nnue_1m_network_tests.py` to verify magic, dimensions, exact payload sizes, and that `current.nnue` is byte-identical to the production `sf250k` baseline.
