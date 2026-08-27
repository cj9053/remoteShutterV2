#include "huffman.h"
#include <map>
#include <queue>

// TODO: implement each function declared in huffman.h.
//
// Reminder of the contract (see huffman.h and the design doc):
//   - build_tree: min-heap, repeatedly merge the two lowest-frequency
//     nodes, until one root remains. Watch the one-symbol edge case.
//   - build_code_table: walk the tree root-to-leaf, left=0, right=1,
//     accumulating (bits, width) per byte value.
//   - huffman_encode/huffman_decode must round-trip exactly.


std::map<uint8_t, int> build_frequency_table(const std::vector<uint8_t>& data){
    std::map<uint8_t, int> frequency_table;
    for(size_t i=0; i<data.size(); i++){
        frequency_table[data[i]]++;
    }
    return frequency_table;
}
