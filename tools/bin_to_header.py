#!/usr/bin/env python3
import sys

def convert(bin_file, out_file):
    with open(bin_file, 'rb') as f:
        data = f.read()
    with open(out_file, 'w', encoding='utf-8') as f:
        f.write('#ifndef NNUE_WEIGHTS_H\n#define NNUE_WEIGHTS_H\n\n')
        f.write('#include <cstddef>\n#include <cstdint>\n\n')
        f.write(f'constexpr std::size_t NNUE_WEIGHTS_SIZE = {len(data)};\n')
        f.write('alignas(64) constexpr std::uint8_t NNUE_WEIGHTS[] = {\n    ')
        for i, byte in enumerate(data):
            f.write(f'0x{byte:02x}')
            if i + 1 != len(data):
                f.write(', ')
            if (i + 1) % 16 == 0 and i + 1 != len(data):
                f.write('\n    ')
        f.write('\n};\n\n#endif // NNUE_WEIGHTS_H\n')

if __name__ == '__main__':
    if len(sys.argv) != 3:
        raise SystemExit('usage: bin_to_header.py input.nnue output.h')
    convert(sys.argv[1], sys.argv[2])
