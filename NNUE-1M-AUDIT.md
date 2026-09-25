# ChessZero v1.0.9 — 1M NNUE audit before Android release

## Release decision for this source package

`nets/ChessZero-v1.0.9-current.nnue` is the known Stockfish-derived `sf250k` baseline. The two new 1M files remain in `nets/` for further experiments, but are not embedded by default until their training/data pipeline is corrected and their playing-strength benchmark is repeated.

## Verified network structure

- `single_1m.nnue`: `CZNNUE32`, 40960 inputs, 256 hidden, 1 output, 10,486,564 bytes.
- `multi_1m.nnue`: `CZNNUE64`, 40960 inputs, L1 256/perspective, 512→8→32→1 head, 10,490,852 bytes.
- `current.nnue`: byte-identical to `sf250k.nnue`.

## Important audit finding

The supplied `multi_1m.nnue` is suspiciously saturated. On the first 1,000 records of the supplied `dataset_1m.czds`, its decoded output was exactly `35.05859375 cp` for all 1,000 positions (standard deviation 0; one unique output). Its L1 bias also contains values up to the int16 maximum `32767`, while the L2 weights are overwhelmingly zero/positive-saturated. This is a strong sign of model collapse/saturation, so RMSE/MAE alone should not be used to promote it to the production APK.

The supplied `single_1m.nnue` produced a non-constant output distribution on the same sample (std ≈ 83.5 cp). That does not by itself establish playing strength.

## Training-pipeline findings

The supplied `gen_dataset_fast.py` creates random legal piece placements and labels them from material difference plus Gaussian noise; it does **not** call Stockfish UCI depth 6. Therefore it does not satisfy a Stockfish-depth-6 labeling requirement even though the result summary describes the data as Stockfish-labeled.

The bundled `train_halfkp_ml.py` previously updated W3/OW before computing downstream gradients through W3, and updated W2 before computing the L1 gradient through W2. That was corrected so all sample gradients are computed from the pre-update parameter state before any parameter update is applied.

## Android packaging

The Android build uses one source of truth: `nets/ChessZero-v1.0.9-current.nnue`. CMake converts it to an ELF object and embeds it directly into both the arm64 OEX executable and JNI library. A raw `.nnue` Android asset is intentionally not duplicated in the APK.

The source package is pinned to compile/target SDK 35, AGP 8.5.1, Gradle 8.7, NDK 29.0.14206865, CMake 3.22.1, arm64-v8a, and JDK 17. Android's current guidance states that Android 15 supports 16 KB page-size devices and recommends AGP 8.5.1+ and NDK r28+ for 16 KB compatibility; this project uses newer NDK r29 plus explicit 16 KB linker flags. 

## What this audit proves

It proves source/package integrity and catches the 1M multi-network collapse/data-label mismatch. It does **not** prove an Elo rating or on-device runtime success. The final Android APK still needs to be built by GitHub Actions and then tested in Chessis/on a 16 KB-capable Android environment.
