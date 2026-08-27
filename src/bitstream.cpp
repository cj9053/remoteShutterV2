#include "bitstream.h"

// TODO: implement BitWriter and BitReader here.
//
// Reminder of the contract (see bitstream.h and the design doc):
//   - MSB-first packing, consistently in both writer and reader
//   - finish() must flush a partial trailing byte, zero-padded
//   - BitReader needs an exact bit_length so it doesn't decode padding
//     as real data
