#include "uci/UCI.h"
#include "engine/Evaluation.h"

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    // Load the embedded NNUE network before entering UCI.
    uc::network().load_embedded_network();

    uc::UCI u;
    u.loop();
    return 0;
}
