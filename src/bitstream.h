#pragma once
#include <cmath>

#include <cstdint>
#include <vector>

// Bit-level writer. Bits are packed MSB-first within each byte.
class BitWriter {
public:
    // Appends a single bit (0 or 1) to the stream.
    void write_bit(int bit);

    // Appends the low `width` bits of `value`, most-significant-bit first.
    // width must be in [0, 32].
    void write_bits(uint32_t value, int width);

    // Flushes any partial trailing byte (zero-padded) and returns the
    // packed buffer. Do not call write_bit/write_bits after finish().
    std::vector<uint8_t> finish();

private:
    int bytePosition=7;
    std::vector<uint8_t> bytes_;
    uint8_t curr_byte_ = 0;
    int bits_in_current = 0; 
};

// Bit-level reader over a byte buffer produced by BitWriter.
class BitReader {
public:
    // `bit_length` is the exact number of meaningful bits in `data` —
    // needed to distinguish real trailing bits from BitWriter's zero-padding.
    BitReader(const std::vector<uint8_t>& data, size_t bit_length);

    // Consumes and returns the next bit (0 or 1).
    int read_bit();

    // Consumes `width` bits, returns them assembled MSB-first.
    uint32_t read_bits(int width);

    bool at_end() const;

private:
    int bytePosition = 7;
    size_t bit_length_=0;
    std::vector<uint8_t> byte_buffer;
    size_t byteIndex=0;
    size_t bitIndex=0;
};
