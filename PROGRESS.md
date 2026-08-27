# pi-image-codec — Progress & Handoff

**Read this file first in any new chat session working on this project.**
It exists so a fresh session can pick up exactly where the last one left
off, with zero lost context.

## The system (how this project is being built — follow this exactly)

This is a **learning project**. The user (some prior C++ exposure,
comfortable with syntax, new to systems programming — pointers/bit
manipulation/concurrency/sockets) is building every component
themselves. The AI's job is **coach, not author**:

- The AI writes: contract headers (function signatures + doc comments,
  no bodies), CMake wiring, test harnesses, and this progress doc.
- The user writes: every line of actual algorithm/logic code (`.cpp`
  bodies).
- Workflow per function: AI states the contract and asks a leading
  question about the next piece of logic → user writes an attempt →
  AI reviews line-by-line, **points out bugs by describing symptoms and
  asking the user to trace through them** (never just hands over the
  fix) → user revises → repeat until correct → move to next function.
- When the user is lost on a concept (not a bug), the AI drops down a
  level and explains the concept with a concrete worked example
  (numbers, tables, ASCII diagrams) before returning to code.
- **Failure mode to avoid:** writing the implementation for the user.
  Even when a fix seems obvious/small, ask a guiding question instead
  of supplying the corrected line. Only exception: pure boilerplate the
  user isn't meant to learn from (CMake files, test harness scaffolding,
  contract declarations) — the AI writes those directly and says so.

## Project overview

Resume/portfolio project. Rewrite of a prior Raspberry Pi build
(`~/Developer/Remote-Camera-Shutter` — SwiftUI app + Flask + gphoto2 on
a Canon 60D) in C++: a from-scratch lossless compressor (LZSS feeding
Huffman coding), a hand-rolled multithreaded HTTP server, and a SwiftUI
client with an independent Swift decoder.

**Full design spec (read this for architecture, all component
contracts, wire format, and the milestone list):**
`docs/superpowers/specs/2026-08-27-pi-image-codec-design.md`

**Milestone order** (from the spec, copied here for convenience):
1. `bitstream` — ✅ **DONE** (see below)
2. `huffman` — 🚧 **IN PROGRESS** (see below)
3. `lzss` — not started
4. Combine LZSS → Huffman pipeline, round-trip tested end-to-end
5. `threadpool` — not started
6. Parallelize LZSS match search; benchmark against milestone 3
7. `camera` (gphoto2 subprocess wrapper) — not started
8. `server` (POSIX sockets, hand-rolled HTTP/1.1) — not started
9. Thread pool wired into the server
10. SwiftUI client + independent Swift decoder
11. Full end-to-end: phone → Pi → compressed response → decoded image

## Build & test

```bash
cd ~/Developer/pi-image-codec
cmake -S . -B build      # first time / after CMakeLists.txt changes
cmake --build build       # after any .cpp/.h change
./build/bitstream_test
./build/huffman_test
```

## Status: milestone 1 (`bitstream`) — DONE, test passing

`src/bitstream.h` / `src/bitstream.cpp` are fully implemented by the
user. `./build/bitstream_test` passes: 500 random `(value, width)` pairs
round-tripped through `BitWriter` → `BitReader` correctly.

**`BitWriter`** (`write_bit`, `write_bits`, `finish`) — packs bits
MSB-first into bytes using a `curr_byte_` scratch byte + `bits_in_current`
counter, pushing to `bytes_` when a byte fills. `bytePosition` (=7) is
the constant top-bit-position-within-a-byte.

**`BitReader`** (constructor, `read_bit`, `read_bits`, `at_end`) —
mirrors the writer: `byte_buffer` (copy of the input), `bit_length_`
(exact valid bit count, distinguishes real data from zero-padding),
`byteIndex`/`bitIndex` tracking read position. `at_end()` compares
`byteIndex*8 + bitIndex` against `bit_length_`.

### Bugs the user hit and fixed while building this (useful pattern
### library for spotting the same mistakes again in huffman/lzss/etc.)

These are **recurring personal mistake patterns**, not one-offs — watch
for them recurring in the next components:

1. **`||` vs `&&` inverted in a guard clause** — `if (bit != 0 || bit != 1)`
   is a tautology (always true), silently disabling the whole function.
   Trace through both branches explicitly when this class of bug is
   suspected.
2. **`==` vs `=`** — wrote `curr_byte_==0;` intending to reset the
   variable; it's a discarded comparison, not an assignment. Also hit
   in the constructor as **parameter/member shadowing**:
   `bit_length = bit_length;` inside the constructor assigned the
   parameter to itself because the member had the same name — fixed by
   renaming the member to `bit_length_` (trailing underscore convention
   used throughout for all members, adopted specifically to prevent this
   class of bug from being possible).
3. **Dead code after `return`** — wrote the bookkeeping (`bitIndex++`,
   rollover `if`) *after* the `return` statement in an early draft of
   `read_bit`; unreachable. Ordering matters: side effects before the
   final `return`.
4. **Off-by-one / wrong-direction loop bounds** — multiple rounds on
   `write_bits`' loop: `i < width - 1` (misses the last bit),
   `i <= width; i--` (infinite loop / negative shift = UB),
   `i <= 0` starting from a positive `i` (loop body never runs once).
   Landed on `for (int i = width - 1; i >= 0; i--)`. **Trace through a
   concrete small example (e.g. width=3) every time a loop bound is in
   doubt** — this was the technique that consistently found the bug.
5. **Reaching into the wrong class's member** — used `BitWriter`'s
   private `bytePosition` from inside `BitReader::read_bit` (different
   class, inaccessible) — fixed by adding an equivalent member to
   `BitReader`.
6. **Type mismatches between header declaration and `.cpp` definition**
   — `u_int8_t` (BSD-ism) vs `uint8_t` (standard, what the header
   declared) in `finish()`'s return type; `int32_t` vs `uint32_t` in
   `read_bits`. Both are "close enough to look right, don't match"
   bugs — always check the `.cpp` signature against the `.h` declaration
   character-for-character when something won't link.
2b. **`const` placement on member functions** — wrote `const bool
   at_end()` (return-type const) instead of `bool at_end() const`
   (the function itself must be `const`, matching the header
   declaration, or it won't be recognized as implementing it).
7. **Confusing "extract a bit" (shift-then-mask, `(value >> i) & 1`,
   used going *out* of a value) with "insert a bit" (shift-then-OR,
   `(acc << 1) | bit`, used building a value *up*)** — came up in both
   `write_bits` (extracting from `value`) and `read_bits` (assembling
   into `assembledBits`); same underlying shift mechanic, opposite
   direction/purpose. Worth re-explaining the distinction if it
   resurfaces.
8. **Signed/unsigned comparison warning** (`byteIndex`/`bitIndex` were
   `int`, compared against `size_t bit_length_`) — resolved by changing
   `byteIndex`/`bitIndex` to `size_t`, matching what they actually
   represent (buffer indices, never negative).

## Status: milestone 2 (`huffman`) — scaffolded, implementation not started

Files created, not yet implemented by the user:
- `src/huffman.h` — full contract, done, do not need to revisit unless
  a design gap is found during implementation.
- `src/huffman.cpp` — empty stub with a reminder comment. **This is
  where the user picks up.**
- `tests/huffman_test.cpp` — written by the AI. Round-trips 4 cases:
  empty input, single-repeated-byte (the one-symbol tree edge case —
  see design doc's pitfalls section), `"ABRACADABRA"`, and 2000 bytes
  of pseudo-random binary data (worst case for compression ratio, still
  must round-trip exactly).
- `CMakeLists.txt` — `huffman` library target (links against
  `bitstream`) and `huffman_test` executable added.

**`HuffmanNode` struct** (already defined in `huffman.h`): `byte`,
`frequency`, `left`/`right` as `shared_ptr<HuffmanNode>`, `is_leaf()`
helper (true when both children are null).

**Functions to implement, in this order** (matches the design doc's
huffman contract section):
1. `build_frequency_table(data) -> map<byte, count>` — **next up, not
   yet started.** Count occurrences of each byte value in `data`.
2. `build_tree(freq_table) -> shared_ptr<HuffmanNode>` — min-heap
   (`std::priority_queue`, already `#include <queue>`'d in the .cpp),
   repeatedly merge two lowest-frequency nodes. Must handle the
   one-symbol case (a tree that can't branch — pick a convention, e.g.
   that byte's code is a single `0` bit).
3. `build_code_table(root) -> map<byte, (code_bits, width)>` — walk
   root-to-leaf, left=0/right=1.
4. `huffman_encode(data, code_table) -> vector<uint8_t>` — use
   `BitWriter` from the now-working `bitstream` component.
5. `huffman_decode(encoded, bit_length, root, original_length) ->
   vector<uint8_t>` — use `BitReader`; walk the tree per bit via
   `read_bit()`, emit a byte at each leaf reached, reset to root,
   until `original_length` bytes have been emitted.

**Not yet designed/scaffolded:** tree serialization
(`serialize_tree`/`deserialize_tree`, mentioned in the design doc's
huffman section) — deliberately deferred until the wire-format/server
integration milestone, since the standalone round-trip test can pass
the same in-memory tree to both encode and decode without shipping it
anywhere. Revisit when starting milestone 4 (combined pipeline) or
milestone 8 (server).

## Immediate next step for a new session

Pick up with: **"Try writing `build_frequency_table` in
`src/huffman.cpp`."** It's the simplest function in this component —
loop over `data`, count occurrences per byte value, return the map.
Good first question to ask the user before they write it: what
container/approach will they use to count occurrences (a
`std::map<uint8_t, int>`, incrementing on each occurrence)?
