#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <vector>

struct HuffmanNode {
    // Only meaningful when this is a leaf (left == nullptr && right == nullptr).
    uint8_t byte;
    int frequency;
    std::shared_ptr<HuffmanNode> left;
    std::shared_ptr<HuffmanNode> right;

    bool is_leaf() const { return left == nullptr && right == nullptr; }
};

// One entry per byte value that actually appears in the input.
std::map<uint8_t, int> build_frequency_table(const std::vector<uint8_t>& data);

// Repeatedly merges the two lowest-frequency nodes until one root remains.
// Precondition: freq_table is non-empty.
std::shared_ptr<HuffmanNode> build_tree(const std::map<uint8_t, int>& freq_table);

// One entry per byte value present in the tree: (code bits, code width).
// Root-to-leaf path, left = 0, right = 1.
std::map<uint8_t, std::pair<uint32_t, int>> build_code_table(
    const std::shared_ptr<HuffmanNode>& root);

// Encodes `data` using `code_table`, returns the packed bitstream.
std::vector<uint8_t> huffman_encode(
    const std::vector<uint8_t>& data,
    const std::map<uint8_t, std::pair<uint32_t, int>>& code_table);

// Decodes `bit_length` bits from `encoded` by walking `root` per bit,
// stopping once `original_length` bytes have been emitted.
std::vector<uint8_t> huffman_decode(
    const std::vector<uint8_t>& encoded,
    size_t bit_length,
    const std::shared_ptr<HuffmanNode>& root,
    size_t original_length);
