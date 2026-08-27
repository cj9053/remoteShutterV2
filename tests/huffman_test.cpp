#include "huffman.h"

#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>
#include <vector>

static bool roundtrip(const std::vector<uint8_t>& data, const char* label) {
    auto freq = build_frequency_table(data);
    auto tree = build_tree(freq);
    auto code_table = build_code_table(tree);

    std::vector<uint8_t> encoded = huffman_encode(data, code_table);

    // Recompute the exact bit length the encoder produced, since the test
    // harness needs it to construct-decode symmetrically to how a real
    // caller would (it would come from wherever the encoder's caller
    // stores/ships it, e.g. a wire-format header).
    size_t bit_length = 0;
    for (uint8_t b : data) {
        bit_length += code_table.at(b).second;
    }

    std::vector<uint8_t> decoded = huffman_decode(encoded, bit_length, tree, data.size());

    if (decoded != data) {
        std::fprintf(stderr, "MISMATCH on case '%s': sizes %zu vs %zu\n",
                     label, data.size(), decoded.size());
        return false;
    }

    std::printf("PASS '%s': %zu bytes -> %zu bytes (%.1f%%)\n",
                label, data.size(), encoded.size(),
                data.empty() ? 0.0 : 100.0 * encoded.size() / data.size());
    return true;
}

int main() {
    bool ok = true;

    // Empty input.
    ok &= roundtrip({}, "empty");

    // Single repeated byte (the one-symbol tree edge case).
    ok &= roundtrip(std::vector<uint8_t>(100, 'A'), "single-symbol");

    // Simple repetitive text.
    {
        std::string s = "ABRACADABRA";
        ok &= roundtrip(std::vector<uint8_t>(s.begin(), s.end()), "abracadabra");
    }

    // Pseudo-random binary data (worst case for compression, still must round-trip).
    {
        std::srand(42);
        std::vector<uint8_t> data(2000);
        for (auto& b : data) b = static_cast<uint8_t>(std::rand() % 256);
        ok &= roundtrip(data, "random-binary");
    }

    return ok ? 0 : 1;
}
