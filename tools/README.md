# ChessZero v0.75 — HalfKP training pipeline

The v0.75 pipeline is deliberately small and reproducible. It uses Python 3
and NumPy only; it does not require `python-chess`.

## 1. Prepare positions

Create a tab-separated text file:

```text
<FEN>\t<target>
```

`target` is from the side-to-move perspective and is normally in `[-1, 1]`.
Use `1` for a win, `0` for a draw and `-1` for a loss.

## 2. Convert to the binary dataset

```bash
python3 tools/halfkp_dataset.py tools/data/sample_positions.txt build/sample.czds
python3 tools/validate_dataset.py build/sample.czds
```

The converter encodes both HalfKP perspectives exactly as `NNUE::featureIndex()`:
`64 * 64 * 10 = 40960` features, with a maximum of 60 active features per
position.

## 3. Train a first network

```bash
python3 tools/train_halfkp.py build/sample.czds build/ChessZero-v0.75-sample.nnue \
  --epochs 3 --seed 7401
```

The output is a `CZNNUE32` file with:

- 40960 HalfKP inputs
- 256 hidden units
- int8 first-layer weights
- int8 output weights
- int16 hidden bias
- output shift 8

The C++ engine can load the generated file directly with `EvalFile`.

This trainer is a correctness/pipeline prototype. It is intentionally not the
final strength trainer planned for v0.75. Large-scale self-play data, batching,
validation splits and stronger optimization belong to later versions.
