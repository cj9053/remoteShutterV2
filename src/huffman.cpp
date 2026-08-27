#include "huffman.h"

#include <queue>

// TODO: implement each function declared in huffman.h.
//
// Reminder of the contract (see huffman.h and the design doc):
//   - build_tree: min-heap, repeatedly merge the two lowest-frequency
//     nodes, until one root remains. Watch the one-symbol edge case.
//   - build_code_table: walk the tree root-to-leaf, left=0, right=1,
//     accumulating (bits, width) per byte value.
//   - huffman_encode/huffman_decode must round-trip exactly.
