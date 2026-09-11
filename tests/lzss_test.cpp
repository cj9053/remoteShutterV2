#include "lzss.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

static bool roundtrip(const std::vector<uint8_t>& data, const char* label) {
    std::vector<Token> tokens = lzss_encode(data);
    std::vector<uint8_t> decoded = lzss_decode(tokens);

    if (decoded != data) {
        std::fprintf(stderr, "MISMATCH on case '%s': sizes %zu vs %zu\n",
                     label, data.size(), decoded.size());
        return false;
    }

    std::printf("PASS '%s': %zu bytes -> %zu tokens\n",
                label, data.size(), tokens.size());
    return true;
}

int main() {
    bool ok = true;

    // Empty input.
    ok &= roundtrip({}, "empty");

    // Fewer bytes than MIN_MATCH -- nothing to match against at all.
    ok &= roundtrip({'A', 'B'}, "shorter-than-min-match");

    // Single repeated byte, long enough to force offset < length
    // (the overlapping-copy trap called out in the design doc).
    ok &= roundtrip(std::vector<uint8_t>(64, 'A'), "repeated-byte");

    // Simple repetitive text with an exact repeated substring.
    {
        std::string s = "ABRACADABRA";
        ok &= roundtrip(std::vector<uint8_t>(s.begin(), s.end()), "abracadabra");
    }

    // Pseudo-random binary data -- worst case for compression (mostly
    // literals), still must round-trip exactly.
    {
        std::srand(42);
        std::vector<uint8_t> data(2000);
        for (auto& b : data) b = static_cast<uint8_t>(std::rand() % 256);
        ok &= roundtrip(data, "random-binary");
    }

    // Long run that exceeds MAX_MATCH in a single reference, forcing the
    // encoder to split it across multiple tokens.
    ok &= roundtrip(std::vector<uint8_t>(600, 'Z'), "longer-than-max-match");

    return ok ? 0 : 1;
}
