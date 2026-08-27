#include "bitstream.h"

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <vector>

int main() {
    std::srand(static_cast<unsigned>(std::time(nullptr)));

    const int NUM_PAIRS = 500;
    std::vector<uint32_t> values;
    std::vector<int> widths;

    BitWriter writer;
    size_t total_bits = 0;

    for (int i = 0; i < NUM_PAIRS; i++) {
        int width = 1 + (std::rand() % 20); // widths 1..20 bits
        uint32_t max_value = (width >= 32) ? 0xFFFFFFFFu : ((1u << width) - 1);
        uint32_t value = static_cast<uint32_t>(std::rand()) & max_value;

        values.push_back(value);
        widths.push_back(width);
        writer.write_bits(value, width);
        total_bits += width;
    }

    std::vector<uint8_t> packed = writer.finish();

    BitReader reader(packed, total_bits);
    for (int i = 0; i < NUM_PAIRS; i++) {
        uint32_t got = reader.read_bits(widths[i]);
        if (got != values[i]) {
            std::fprintf(stderr,
                "MISMATCH at pair %d: wrote %u (width %d), read back %u\n",
                i, values[i], widths[i], got);
            return 1;
        }
    }

    std::printf("PASS: %d round-tripped (value, width) pairs, %zu total bits, %zu packed bytes\n",
                NUM_PAIRS, total_bits, packed.size());
    return 0;
}
