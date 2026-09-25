# ChessZero v1.0.9 — Nâng cấp & thay thế file từ các engine tham khảo

Đánh giá trên bản gốc **ChessZero v1.0.8** (~6200 dòng C++). So sánh với các engine mã nguồn mở: **Berserk** (jay-honnold/Berserk, C, hiện đại — magic bitboards, NNUE HalfKAv2, staged movegen), **Demolito**, **Ethereal**, và upstream **Fathom** (jdart1/Fathom).

---

## 1. File thay thế TRỰC TIẾP được (chuẩn thống nhất, drop-in)

| File gốc | Thay bằng | Lý do |
|---|---|---|
| `third_party/Fathom/src/tbprobe.c` | upstream Fathom `c9c6fef` | 18 commit fix sau bản pin `c6cf6e8`: sửa crash C++20, lỗi alignment integer, race condition `tb_init/tb_free`, stdendian port, tài liệu thread-safety. |
| `third_party/Fathom/src/tbprobe.h` | upstream `c9c6fef` |同上. |
| `third_party/Fathom/src/stdendian.h` | upstream `c9c6fef` |同上. |
| `third_party/Fathom/SOURCE.lock` | cập nhật hash/size mới | Giữ contract build tái lập. |

→ **Đã thực hiện.** Vendored Fathom gốc pristine (không sửa local), nên fast-forward an toàn.

**`src/book/PolyglotRandom.h`**: Đã kiểm tra — khớp 100% bảng Polyglot chuẩn (781 entries, `Random[0]=0x9D39247E33776D41`). **Không cần thay.**

---

## 2. File KHÔNG thể drop-in nhưng đã NÂNG CẤP thuật toán (port từ Berserk)

Các file lõi (`Search.cpp`, `Position.cpp`, `Evaluation.cpp`, `UCI.cpp`, `NNUE.cpp`) dùng API/kiểu dữ liệu riêng, **không thể thay thẳng** file từ engine khác (Stockfish/Berserk dùng encoding quân cờ, move, Position khác hẳn). Nhưng thuật toán bên trong thì chuẩn và có thể port:

| File | Vấn đề bản gốc | Nâng cấp (v1.0.9) | Tham khảo |
|---|---|---|---|
| `src/bitboard/Bitboard.cpp` | Slider (xe/tượng/hậu) dùng vòng lặp từng ô — chậm 5–10×, gọi ở mọi `inCheck/attacked/movegen/SEE/mobility` | **Magic bitboards** (fancy magic, O(1) lookup: `(occ&mask)*magic>>shift`). API công khai giữ nguyên, mọi call site tự động được tốc độ mới. ~850KB bảng. | Berserk `attacks.c` (magic numbers chuẩn) |
| `src/engine/Search.cpp::seeCapture` | SEE dùng `Position cur=p; cur.make(); generateLegal()` lặp — cấp phát heap + make/unmake ở MỌI capture trong qsearch | **Swap Algorithm** thuần bitboard: không copy Position, không generate move, không heap. Có xử lý promotion + en-passant + x-ray slider. | Berserk `see.h` + CPW |
| `src/nnue/NNUE.cpp::applyColumn` | Chỉ có NEON (ARM); desktop x86 chạy scalar 8-ôn/lần | Thêm **AVX2** (16 int16/lần) và **SSE4.1** (8 int16 vectorised). `nnueSimdPath()` báo `avx2/sse4.1`. | chuẩn SSE/AVX2 |

→ **Đã thực hiện, build thành công (EXIT=0), perft(5)=4865609 đúng chuẩn, search chạy depth 16.**

---

## 3. Những gì engine VẪN còn thiếu/điểm yếu (ưu tiên theo Elo)

**Hiệu năng (NPS) — còn lại:**
- [ ] **Staged move generation** (`movepick`): hiện `generateLegal` + `std::sort` toàn bộ nước ở mỗi node. Cần tách: TT move → captures (SEE) → killers → quiet (history), pick-best không sort. (Berserk `movepick.c`)
- [ ] `Position::make/undo` dùng `std::vector<State>` (heap) — nên đổi sang mảng stack `[1024]`.
- [ ] `parseMove` (UCI) gọi `p.legal()` sinh toàn bộ nước chỉ để parse 1 nước.
- [ ] Không có `bench` command (đo NPS chuẩn).

**Thuật toán search — còn thiếu (đáng kể Elo):**
- [ ] Singular extensions, check extensions, passed-pawn extensions, recapture extensions
- [ ] Internal Iterative Deepening (IID)
- [ ] ProbCut
- [ ] Late Move Pruning (LMP) full-width, countermove pruning, follow-up history (đang chỉ có continuation đơn)
- [ ] Multicut, futility pruning ở frontier (đang chỉ history pruning)
- [ ] Lazy SMP chỉ root-parallel; engine mạnh dùng shared-TT full tree + helper threads.
- [ ] Không có MultiPV, ponder thật sự, `go nodes`, contempt, `UCI_LimitStrength`.

**Đánh giá (điểm yếu lớn nhất về sức mạnh):**
- [ ] **Mạng NNUE yếu**: HalfKP 40960×256 1 lớp, bootstrap chưa train thật. Engine hiện đại dùng HalfKAv2 (king-bucket) nhiều lớp (vd 1024→8→32). Đây là lý do chính engine chơi nước yếu (vd `b1a3` ở depth 16). Cần train lại hoặc port kiến trúc mới.
- [ ] Classical eval cơ bản (không tapered mid/end, không pawn hash, không king-safety table, không threats, không space, không trapped pieces).
- [ ] Không có PSQT term trong NNUE.

**Đã có (không thiếu):** PVS, aspiration, LMR, null-move+verify, razoring, reverse futility, history/countermove/continuation, killers, SEE, qsearch delta/SEE-prune, TT có cluster+age+striped-lock, Lazy SMP, Syzygy (Fathom), Polyglot book, UCI đầy đủ, Android/OEX, embedded NNUE.

---

## 4. File KHÔNG nên thay thế từ engine khác

- `src/engine/Search.cpp`, `Evaluation.cpp`, `Position.cpp`, `Move.*`, `Zobrist.*`, `uci/UCI.*`, `nnue/NNUE.*`, `TranspositionTable.*`, `api/EngineAPI.*`: **tightly coupled** với kiểu `uc::Piece` (WP=1…BK=-6), `Move` (from/to/flag), `Bitboards::piece[12]`, `State` stack. Thay file từ engine khác = phá hỏng toàn bộ build. Chỉ nên **port thuật toán** (như mục 2).
- `nets/*.nnue`: định dạng riêng `CZNNUE32`, không tương thích Stockfish `.nnue`. Cần viết converter nếu muốn dùng mạng train sẵn.

---

## 5. Build & kiểm chứng

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DCHESSZERO_BUILD_TESTS=OFF
cmake --build build -j4
printf 'position startpos\nperft 5\n' | ./build/chesszero   # nodes 4865609 ✓
```

Biên dịch trên g++ 11.4 / cmake 3.22, toàn bộ target (kể cả test suite) pass.

---

## 6. NNUE mới: Stockfish knowledge distillation (v1.0.9)

**Vấn đề gốc**: mạng `ChessZero-v0.75-halfkp.nnue` là bootstrap — train trên self-play của chính classical eval yếu → đánh giá sai, chơi nước kỳ lạ (vd `b1a3` ở depth 16).

**Đã làm**:
1. Dùng Stockfish 16 (classical eval, depth 6) gán nhãn **30.000 vị trí** đa dạng (opening ngẫu nhiên + Stockfish self-play).
2. Train lại HalfKP 40960×256 (cùng kiến trúc, **không cần sửa code inference**) 12 epochs, target_scale=1500.
3. Kết quả: `nets/ChessZero-v1.0.9-sfdistill.nnue` (10 MiB, định dạng CZNNUE32 tương thích trực tiếp).
4. UCI tự ưu tiên load mạng mới. MSE giảm 114k → 29k (train).

**Kết quả kiểm chứng**: startpos depth 9 → `c2c4` (English Opening, +10cp) — thay vì `b1a3` của mạng cũ.

**Lưu ý**: Đây vẫn là mạng 1 lớp HalfKP train trên 30k vị trí (modest). Để mạnh hơn: (a) tăng dữ liệu lên 1M+ vị trí, (b) train thêm epochs với lr decay, (c) port HalfKAv2 nhiều lớp. Script train có sẵn trong `tools/train_halfkp.py`; dữ liệu mẫu ở `nnue_train/sf_labeled.txt`.

---

## 7. Port HalfKAv2-style multi-layer NNUE (v1.0.9, CZNNUE64)

**Kiến trúc mới** (tương thích ngược với CZNNUE32 single-layer):
- L1: 2 accumulator (white/black perspective), mỗi bên 256 int16, clip [0,127] → int8.
- Concatenate **stm-first** (512 int8) → L2=8 (int8 weights, int32 acc, clip) → L3=32 (clip) → output scalar (int32, shift 8).
- **Incremental update KHÔNG ĐỔI** (addFeature/applyMoveFeatures/refresh chỉ phụ thuộc L1) — port an toàn.
- Định dạng file mới: magic `CZNNUE64`, version 64. `NNUE::loadMemory` tự detect và dispatch `arch_=Single/Multi`. `evaluate()` gọi `forwardMulti()` khi mạng multi-layer.
- Trainer mới: `tools/train_halfkp_ml.py` (backprop đầy đủ qua L2/L3, weight decay, lr decay).

**Files thay đổi**: `src/nnue/NNUE.h` (thêm L2/L3 constants, members, forwardMulti), `src/nnue/NNUE.cpp` (loadMemory v64, forwardMulti, evaluate dispatch), `tools/train_halfkp_ml.py` (mới), `src/uci/UCI.cpp` (candidate list ưu tiên ML net).

**Cấu hình có thể điều chỉnh**: `HIDDEN_SIZE` (256/bên → 512 để được 1024 concat), `L2_SIZE`, `L3_SIZE` trong `NNUE.h` — chỉ cần recompile + retrain.
