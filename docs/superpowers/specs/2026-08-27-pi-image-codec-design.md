# pi-image-codec — Design Document

## Purpose and approach

A resume/portfolio project, and a learning project. Follow-up to a prior
Raspberry Pi build (dnsmasq/DHCP hotspot + Flask REST server driving a
Canon 60D over `gphoto2`, see `Remote-Camera-Shutter`). This version
rewrites the pipeline in C++: a lossless compressor built from scratch
(LZSS feeding Huffman coding), a hand-rolled multithreaded HTTP server,
and a SwiftUI client that reverses the compression and renders the image.

**This is not a from-scratch general image codec.** We are not doing
color-space transforms, DCT, or quantization — the compressor treats the
captured JPEG/CR2's bytes as an opaque byte stream and shrinks them
losslessly. That's a different (simpler, still legitimate) problem than
JPEG's lossy pixel-transform approach, and it keeps the scope aimed at
the systems/threading/networking skills this project is meant to show.

**Problem-solving approach: stepwise refinement.** Break the system into
independent components, each solving one sub-problem, each testable in
isolation before anything is wired together. A component's *interface*
(its function contracts) should be understandable without reading its
implementation, and its implementation should be changeable without
breaking anything that calls it. Two components should never share a
secret (e.g., both `huffman` and `lzss` reaching into the same mutable
byte buffer's internal layout) — each owns its own representation.

**How to use this document as a learning tool:** each component section
below gives you the *contract* (arguments, return value, failure modes)
the way the original project's `bitpack` spec did — not the
implementation. Write the implementation yourself; use the contract and
the algebraic laws to know when it's correct. Build and round-trip-test
each component standalone before wiring it into the next one — this is
also the milestone order at the bottom of this document.

## Architecture

```
[SwiftUI iPhone App] --HTTP POST /capture--> [Raspberry Pi: C++ server]
                                                     |
                                                     v
                              gphoto2 subprocess captures + downloads JPEG
                                                     |
                                                     v
                                LZSS encode (parallel window search)
                                                     |
                                                     v
                          Huffman encode (parallel by block, over LZSS output)
                                                     |
                                                     v
                              [header][compressed payload] in HTTP response
                                                     |
        <---- Huffman decode -> LZSS decode ----------
        render UIImage in SwiftUI
```

Each arrow above is a component boundary. Component list, in the order
you'll build them:

1. `bitstream` — bit-level read/write over a byte buffer
2. `huffman` — canonical Huffman coding
3. `lzss` — sliding-window match compression
4. `threadpool` — reusable worker pool
5. `camera` — `gphoto2` subprocess wrapper
6. `server` — POSIX socket HTTP server
7. Swift decoder — independent reimplementation of the decode path

## Component contracts

### 1. `bitstream`

Everything downstream produces or consumes a stream of individual bits,
not bytes — Huffman codes and LZSS back-references are rarely a whole
number of bits, let alone a whole byte. `bitstream` is the single place
that knows how bits get packed into bytes, so nothing else has to.

**`BitWriter`**
- `write_bit(bit: 0 or 1) -> void` — appends one bit to the stream.
- `write_bits(value: unsigned, count: width in bits) -> void` — appends
  the low `width` bits of `value`, most-significant-bit first.
- `finish() -> vector<byte>` — flushes any partial trailing byte
  (zero-padded) and returns the packed buffer. Calling `write_bit` after
  `finish()` is a contract violation (undefined — document what your
  implementation does; don't silently continue accepting writes).

**`BitReader`**
- constructed from a `vector<byte>` plus the **exact bit length** to
  read (you need this because the trailing byte may be zero-padded —
  without knowing the true bit count, you can't tell padding zero-bits
  from real ones).
- `read_bit() -> 0 or 1` — consumes and returns the next bit.
- `read_bits(count: width) -> unsigned` — consumes `width` bits, returns
  them assembled MSB-first as an unsigned value.
- `at_end() -> bool`.

**Algebraic law (your test oracle):** for any sequence of writes,
reading back with the matching sequence of `read_bits(width)` calls
must reproduce the original values exactly. Concretely: write N random
`(value, width)` pairs, `finish()`, construct a `BitReader` over the
result, `read_bits(width)` N times, assert each equals the original
`value`.

**Traps:**
- Off-by-one on which end is "most significant" — pick MSB-first and
  apply it consistently in *both* writer and reader, or every other
  component built on top inherits a silent bug.
- Forgetting to mask `value` to `width` bits before writing — a caller
  passing a `value` that doesn't fit should either be truncated
  deliberately (document it) or rejected; don't let it silently corrupt
  neighboring bits.
- The final partial byte: decide what the padding bits are (zero is the
  standard choice) and never accidentally decode them as data — this is
  exactly why `BitReader` needs an explicit bit-length, not just a byte
  buffer.

### 2. `huffman`

- `build_frequency_table(data: byte buffer) -> map<byte, count>`
- `build_tree(freq_table) -> Tree` — combine the two lowest-frequency
  nodes repeatedly (a min-heap) until one node remains. Tie-breaking
  rule matters for determinism — pick one (e.g. lower byte value wins
  ties) and document it, or your compressed output won't be
  reproducible run-to-run for the same input, which will confuse your
  own testing.
- `build_code_table(tree) -> map<byte, (bits, width)>` — root-to-leaf
  path per byte value.
- `serialize_tree(tree) -> byte buffer` and
  `deserialize_tree(buffer) -> Tree` — the decoder needs the same tree
  the encoder used; ship it. (A simple pre-order traversal encoding
  works: one bit says "leaf, followed by the 8-bit byte value" or
  "internal node, recurse left then right.")
- `encode(data, code_table) -> BitWriter output`
- `decode(bitstream, tree) -> byte buffer` — walk the tree per bit,
  emit a byte at each leaf, reset to root, repeat until the declared
  original length is reached (you need the original *byte* length
  too, separately from the bitstream's bit length — decoding stops by
  byte count, not by "bits ran out," because of trailing padding).

**Algebraic law:** `decode(encode(data)) == data`, byte for byte, for
any input including the empty buffer and a single repeated byte
(frequency table with one entry — this is a real edge case: a tree
with one leaf needs a defined convention, e.g. that byte's code is a
single `0` bit, since a tree can't branch with only one symbol).

**Traps:**
- The one-symbol edge case above — it will not occur to you until you
  test an all-zeros buffer and get a division-by-zero or infinite loop
  in tree construction.
- Tree serialization format mismatch between encode and decode — same
  class of bug as the bitstream MSB/LSB trap: decide the format once,
  test it independently of the rest of Huffman (serialize a tree,
  deserialize it, assert structural equality, before ever encoding real
  data with it).

### 3. `lzss`

Operates on the byte stream, replacing repeated runs with
back-references. Parameters (fix these as constants first, don't make
them configurable until the basic version works):
- `WINDOW_SIZE` — how far back a match can point (e.g. 4096 bytes)
- `MIN_MATCH` — shortest match worth encoding as a reference instead of
  literals (e.g. 3 bytes — below this, a back-reference costs more bits
  than just emitting the literals)
- `MAX_MATCH` — longest single match (bounded by how many bits you
  spend encoding length)

**Token stream (the intermediate representation, before Huffman):**
each token is either a literal byte or a `(offset, length)` pair
pointing backward into already-decoded output.

- `find_longest_match(window, lookahead) -> (offset, length)` — the
  core search: for the current position, find the longest match to
  earlier data within `WINDOW_SIZE`. This is the function you'll
  parallelize later (split the window into chunks, search each chunk on
  a worker thread, reduce to the longest match found across all
  chunks).
- `encode(data) -> vector<Token>`
- `decode(tokens) -> byte buffer` — replay literals verbatim; for a
  `(offset, length)` token, copy `length` bytes starting `offset` bytes
  back **in the output being built**, not the input (this matters:
  matches can overlap the position being written, e.g. encoding "AAAA"
  as one literal `A` plus a match with offset=1, length=3 — decode must
  copy byte-by-byte from the growing output, not `memcpy`, or an
  overlapping-region copy will read data that doesn't exist yet).

**Algebraic law:** `decode(encode(data)) == data`, for repetitive data
(where matches actually fire) and for high-entropy/random data (where
they mostly don't — confirms the "fall back to literals" path works).

**Traps:**
- Encoding a match at all when `length < MIN_MATCH` — check this
  boundary explicitly; the naive version happily emits a 2-byte match
  that costs more than 2 literal bytes would.
- The overlapping-copy bug described above — write a targeted test
  for a run of a single repeated byte (`"AAAAAAAA"`) specifically
  because it forces `offset < length`.
- Window boundary at the very start of the file — nothing to match
  against yet; the first `MIN_MATCH` bytes or so must always be
  literals. An off-by-one here shows up as a crash or a corrupted first
  few bytes, easy to miss if your test data happens to start with
  unique bytes.

### 4. `threadpool`

- constructed with a worker count (`N`), spawns `N` threads.
- `submit(task: callable) -> future<result>` — enqueues work.
- Internally: a task queue guarded by a mutex, a condition variable
  workers wait on when the queue is empty, workers loop pop-and-execute
  until told to shut down.
- destructor: signal shutdown, join all threads (no dangling threads on
  program exit).

**Algebraic law:** submit N tasks that each record their own thread id
into a thread-safe collection; after all N complete, assert every task
ran exactly once and more than one distinct thread id appears (proves
work was actually distributed, not silently run on one thread).

**Traps:**
- Losing a wakeup: a worker checks the queue, finds it empty, and goes
  to sleep — but a task was enqueued in the gap between the check and
  the sleep. This is exactly what the mutex+condition-variable pairing
  exists to prevent; look up "spurious wakeup" and "lost wakeup" if your
  pool occasionally hangs with tasks still queued.
- Shutdown race: threads must wake up and exit even when the queue is
  empty and no more tasks are coming — the shutdown flag needs the same
  mutex/condvar protection as the queue itself.

### 5. `camera`

- `capture() -> byte buffer, or an error` — spawns
  `gphoto2 --capture-image-and-download` (or your v1 project's exact
  invocation) as a subprocess, waits for it to exit, checks the exit
  code, reads the downloaded file into memory.
- Failure modes to handle explicitly (not crash on): `gphoto2` not
  found, non-zero exit (camera disconnected/busy), file not present
  after a reported success.

No algebraic law here (it's talking to real hardware) — the test is
manual: run it standalone against the physical Canon 60D and confirm a
valid JPEG comes back (openable, sane byte count).

### 6. `server`

- POSIX sockets: `socket` → `bind` → `listen` → `accept` loop.
- Minimal HTTP/1.1 parsing: read until `\r\n\r\n` (end of headers),
  recognize `POST /capture`, ignore/discard the request body (nothing
  needed from it for this endpoint), respond with a status line,
  `Content-Type: application/octet-stream`, `Content-Length`, and the
  compressed body.
- Each accepted connection's handling (parse → capture → compress →
  respond) is submitted to the `threadpool` instead of run inline or
  spawned as a raw thread.

**Traps:**
- A client that never sends `\r\n\r\n` (or sends it split across
  multiple `recv()` calls) — your parser must accumulate across reads,
  not assume one `recv()` returns a complete request.
- Forgetting `Content-Length` (or getting it wrong) — the client's HTTP
  stack won't know when the body ends.

### 7. Swift decoder

A **separate implementation** of Huffman-decode and LZSS-decode in
Swift — not a bridge to the C++ code. Verify compatibility by testing
the Swift decoder against fixture files produced by the C++ encoder
(check a handful of `.bin` compressed outputs into the repo as test
data), rather than by sharing code.

## Wire format

```
[4 bytes] magic ("PICZ")
[4 bytes] original (post-LZSS-pre-Huffman token stream) byte length, big-endian u32
[serialized Huffman tree]
[Huffman-encoded bitstream of the LZSS token stream]
```

Big-endian, matching the convention in the reference handout — pick one
and be consistent between the C++ encoder and Swift decoder; this is
the same class of bug as the bitstream MSB/LSB trap, just at the
message-format level instead of the bit level.

## Testing plan

Plan to spend more time on tests than on the "happy path" implementation
— this mirrors real experience on this kind of project: once
`bitstream`, `huffman`, and `lzss` each pass their round-trip tests in
isolation, wiring the full pipeline together is comparatively easy, and
almost all the real bugs surface during isolated testing, not
integration.

Per component: a standalone CLI test harness (e.g.
`huffman_test roundtrip <file>`) exercising the algebraic law given
above, before that component is ever called from anywhere else.

Integration test: full pipeline against a real captured image —
compressed size reported (should be meaningfully smaller for a real
photo; if it isn't, something's wrong before you optimize anything),
round-trip verified byte-identical.

## Common mistakes to watch for

(Modeled on the reference handout's "traps and pitfalls" — these are
the mistakes most likely to eat your time on *this* project
specifically, not a restatement of the component-level traps above.)

- Testing `bitstream`/`huffman`/`lzss` only on small, convenient inputs
  (e.g. "hello world") — test on binary data (an actual image file),
  the empty buffer, and a single repeated byte; these hit different
  edge cases.
- Two components silently agreeing on an assumption neither states
  explicitly (e.g. `server` assuming `huffman::encode` never returns an
  empty bitstream) — if a component depends on something about
  another's output, write it into that component's contract, don't
  leave it implicit.
- Shipping the Huffman tree once and reusing it for every captured
  image — each image has its own byte-frequency distribution; the tree
  must be rebuilt (and reshipped) per image, or compression will be
  poor and occasionally wrong (bytes the tree wasn't built for).
- Benchmarking the parallel LZSS search against nothing — measure the
  single-threaded version's time on the same input first, so the
  threading milestone has something concrete to compare against.

## Milestones (learning checkpoints)

1. `bitstream` — round-trip tested
2. `huffman` — single-threaded, round-trip tested on a file
3. `lzss` — single-threaded, round-trip tested
4. Combine: LZSS → Huffman pipeline, round-trip tested end-to-end
5. `threadpool` — tested with synthetic tasks
6. Parallelize LZSS match search; benchmark against milestone 3
7. `camera` — tested against physical hardware
8. `server` — single-threaded first, tested with `curl`
9. Thread pool wired into the server for concurrent connections
10. SwiftUI client + Swift decoder, tested against fixtures from
    milestone 4
11. Full end-to-end: phone app → Pi → compressed response → decoded
    image on screen
