#include <iostream>
#include <vector>
#include <string>
#include <cassert>
#include <cmath>

// Khai báo các thư viện nội bộ của ChessZero theo kiến trúc mã nguồn
#include "../src/chess/Position.h"
#include "../src/engine/Search.h"
#include "../src/engine/TranspositionTable.h"
#include "../src/nnue/NNUE.h"

// Mảng 20 FEN phức tạp ép engine sinh nhiều nhánh và tính toán Eval sâu
const std::vector<std::string> REGRESSION_FENS = {
    "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1", // 1. Startpos
    "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1", // 2. Kiwipete
    "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1", // 3. Endgame
    "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1", // 4. Tactical
    "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8", // 5. Promotion/Under-promotion
    "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10", // 6. Symmetrical
    "8/8/8/8/4N3/8/8/1k2K3 w - - 0 1", // 7. Knight moves
    "2rq1rk1/pp1bppbp/2np1np1/8/3NP3/1BN1BP2/PPPQ2PP/R3K2R b KQ - 6 11", // 8. Sicilian Dragon
    "4r1k1/1p3p1p/p1b3p1/8/3R1P2/2N3P1/PP5P/6K1 w - - 1 25", // 9. Rook + Bishop endgame
    "6k1/p4p1p/1p4p1/8/1P3P2/P2R2P1/4r2P/6K1 b - - 0 29", // 10. Rook endgame
    "rnbqkb1r/pppp1ppp/4pn2/8/2P5/2N5/PP1PPPPP/R1BQKBNR w KQkq - 2 3", // 11. English Opening
    "r1bqk2r/pp2bppp/2n1p3/3p4/3P4/3BPN2/PP3PPP/R1BQK2R w KQkq - 0 9", // 12. French Defense structure
    "r2q1rk1/pp1b1ppp/2n1pn2/1B1p4/3P4/4PN2/PP1N1PPP/R2Q1RK1 w - - 1 11", // 13. Queen's Gambit Declined
    "8/5k2/8/8/8/3K4/8/8 w - - 0 1", // 14. Bare Kings (Should evaluate to 0 immediately)
    "8/8/8/4p3/5k2/8/4K3/8 w - - 0 1", // 15. K+P vs K
    "rnbqkbnr/pppp1ppp/8/4p3/4P3/8/PPPP1PPP/RNBQKBNR w KQkq - 0 2", // 16. 1. e4 e5
    "r1bqkbnr/pppp1ppp/2n5/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 2 3", // 17. 1. e4 e5 2. Nf3 Nc6
    "r1bqkb1r/pppp1ppp/2n2n2/4p3/4P3/5N2/PPPP1PPP/RNBQKB1R w KQkq - 4 4", // 18. Two Knights Defense
    "r1bqkb1r/pppp1ppp/2n2n2/4p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 5 4", // 19. Italian Game
    "r1bqk2r/pppp1ppp/2n2n2/2b1p3/2B1P3/5N2/PPPP1PPP/RNBQK2R w KQkq - 6 5" // 20. Giuoco Piano
};

// Hàm tiện ích kẹp (clamp) điểm số để chống tràn
bool is_valid_score(int score) {
    // Điểm số hợp lệ không bao giờ vượt qua biên độ MATE
    const int MATE_BOUND = 32000;
    return score > -MATE_BOUND && score < MATE_BOUND;
}

void run_v033_regression_tests() {
    std::cout << "========== RUNNING V0.33 REGRESSION TESTS ==========" << std::endl;
    
    // 1. Kiểm tra khởi tạo mạng NNUE
    bool nnue_loaded = NNUE::init("nets/ChessZero-v0.32-demo.nnue");
    assert(nnue_loaded && "NNUE must be loaded successfully to run v0.33 tests.");
    std::cout << "[OK] NNUE Net Loaded safely." << std::endl;

    // 2. Khởi tạo TT Table với giới hạn RAM chặt chẽ (32MB) cho Mobile/Termux
    TT.resize(32);
    std::cout << "[OK] Transposition Table initialized to 32MB." << std::endl;

    Search::Limits limits;
    limits.depth = 10; // Ép độ sâu 10 để kiểm tra tải RAM và Quantization

    int passed_fens = 0;

    for (size_t i = 0; i < REGRESSION_FENS.size(); i++) {
        Position pos;
        pos.set_fen(REGRESSION_FENS[i]);

        std::cout << "Testing FEN " << (i + 1) << "/20... ";

        // Đảm bảo Accumulator được làm mới (refresh) đúng với vị trí ban đầu
        NNUE::refresh_accumulator(pos);

        // Bắt đầu quá trình tìm kiếm, bắt lỗi nếu Crash/OOM xảy ra
        try {
            int score = Search::go(pos, limits);

            // Kiểm tra chống tràn số học (Integer Overflow/NaN)
            if (!is_valid_score(score)) {
                std::cout << "[FAIL] Score out of bounds (Overflow detected): " << score << std::endl;
                assert(false);
            }

            // In ra kết quả rút gọn nếu thành công
            std::cout << "[PASS] Depth: 10 | Score: " << score << std::endl;
            passed_fens++;

        } catch (const std::bad_alloc& e) {
            // Phát hiện lỗi Out Of Memory (OOM) - Cấp phát động rò rỉ trong Hot-path
            std::cout << "[FAIL OOM] Memory allocation failed on FEN " << (i + 1) << std::endl;
            assert(false);
        } catch (...) {
            // Phát hiện các lỗi sập nguồn (Crash) không xác định
            std::cout << "[FAIL CRASH] Unknown exception occurred on FEN " << (i + 1) << std::endl;
            assert(false);
        }
    }

    std::cout << "====================================================" << std::endl;
    if (passed_fens == 20) {
        std::cout << "SUCCESS: All 20 FENs passed depth 10 without OOM or Overflow." << std::endl;
        std::cout << "ChessZero is stable for Mobile (v0.33 Requirement Met)." << std::endl;
    } else {
        std::cout << "FAILED: Only " << passed_fens << "/20 FENs passed." << std::endl;
    }
}

int main() {
    // Kích hoạt bộ kiểm tra
    run_v033_regression_tests();
    return 0;
}