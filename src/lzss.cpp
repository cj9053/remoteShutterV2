#include "lzss.h"

// TODO: implement each function declared in lzss.h.
//
// Reminder of the contract (see lzss.h and the design doc):
//   - find_longest_match: scan backward from pos, within WINDOW_SIZE,
//     for the longest run that also occurs starting at pos. Cap length
//     at MAX_MATCH and at the bytes actually remaining in data.
//   - lzss_encode: walk the buffer; at each position try
//     find_longest_match. If it found >= MIN_MATCH, emit a match token
//     and advance by the match length; otherwise emit one literal token
//     and advance by 1.
//   - lzss_decode: replay tokens into a growing output buffer. Match
//     tokens must copy byte by byte from the *output*, not the input --
//     watch the overlapping-copy case (offset < length).
