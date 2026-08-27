# pi-image-codec — Design Spec

## Purpose

A resume/portfolio project. Follow-up to a prior Raspberry Pi project (dnsmasq/DHCP hotspot + Flask REST server for remote camera control). This time: rewrite the pipeline in C++ from scratch, implementing a real lossless compression algorithm (LZSS + Huffman) and a hand-rolled multithreaded HTTP server, with a SwiftUI client that decompresses and displays the image.

This is a **learning project**. The user has some C++ exposure (syntax-comfortable, not systems-programming-fluent) and wants to be guided through building each piece themselves — not handed a finished implementation. The spec below defines the target architecture; the implementation plan (next step, via writing-plans) breaks it into teaching checkpoints.

## Non-goals

- Not reproducing a general-purpose image format (no chroma subsampling, no JPEG-style DCT/lossy compression) — this is a lossless byte-stream compressor, applied to whatever raw/JPEG bytes gphoto2 hands back.
- Not building a production-hardened HTTP server (no TLS, no arbitrary-method routing, no chunked transfer encoding) — one POST endpoint, minimal but correct HTTP/1.1 parsing.
- Not doing camera driver work — capture is delegated to `gphoto2` as a subprocess, same as the v1 project.

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
        <---- Huffman decode -> LZSS decode ---------
        render UIImage in SwiftUI
```

## Components

Each component is built and round-trip-tested standalone (via a small CLI harness) before being wired into anything else. This is both the testing strategy and the teaching checkpoint structure.

1. **`bitstream.h/.cpp`** — bit-level reader/writer over a byte buffer. Foundation for both Huffman and LZSS encoding, since neither produces byte-aligned output. Test: write a known bit pattern, read it back, assert equality.

2. **`huffman.h/.cpp`** — canonical Huffman coding.
   - Build frequency table over input bytes
   - Build the Huffman tree (min-heap by frequency)
   - Generate code table (bit patterns per byte value)
   - Encode: replace each byte with its code, pack via `bitstream`
   - Decode: walk the tree per bit until a leaf, emit byte, repeat
   - Test: compress/decompress a file, assert byte-identical round trip; assert compressed size < original for realistic (non-random) input.

3. **`lzss.h/.cpp`** — sliding-window LZ compression.
   - Fixed-size search window (e.g. 4KB) and lookahead buffer
   - Match finder: for the current position, find the longest match in the window (this is the part that gets threaded — see below)
   - Emit (offset, length) back-references or literal bytes
   - Decode: replay literals and back-references against a growing output buffer
   - Test: same round-trip + size assertions as Huffman, on data with repetition (e.g. a screenshot or synthetic repeated pattern) to confirm matches are actually found.

4. **`threadpool.h/.cpp`** — small reusable fixed-size thread pool (worker threads + task queue + mutex/condition_variable). Two consumers:
   - LZSS match search: split the lookahead/window space into chunks, search each chunk on a worker, reduce to the best match
   - HTTP server: each accepted connection is dispatched to the pool instead of spawning a thread per connection
   - Test: submit N tasks that record which thread ran them, assert all completed and were distributed across workers.

5. **`camera.h/.cpp`** — wraps a subprocess call to `gphoto2 --capture-image-and-download`, waits for exit, reads the resulting file from disk into an in-memory buffer. Test: run standalone against the physical Canon 60D, confirm a valid JPEG buffer comes back.

6. **`server.cpp`** — POSIX socket server (`socket`/`bind`/`listen`/`accept`). Minimal hand-rolled HTTP/1.1 request-line + header parsing — enough to recognize `POST /capture` and know when the request is complete. Each connection handed to the thread pool. Response: raw compressed bytes with a small custom header (see Wire Format), `Content-Type: application/octet-stream`.

7. **Swift decoder (in the iOS app)** — a Swift port of the LZSS+Huffman decode path, kept as a **separate implementation** from the C++ encoder (no C++/Swift bridging). Decodes the response body, reconstructs the image, renders via `UIImage(data:)`.

## Wire format

```
[4 bytes] magic ("PICZ")
[4 bytes] original byte length (uint32, for decode buffer pre-allocation)
[Huffman code table, serialized]
[Huffman-encoded bitstream of the LZSS token stream]
```

The Huffman code table must be shipped with each response (no shared/static table) since it's built per-image from that image's byte frequencies.

## Error handling

- `gphoto2` subprocess failure (camera disconnected, busy, etc.) → server returns a distinct HTTP status (e.g. 502) with a short plaintext error body, no compression attempted.
- Malformed/incomplete HTTP request → connection closed, no crash.
- Decode-side corruption (SwiftUI receives a truncated/invalid stream) → decoder returns an error/nil rather than crashing; app shows a retry affordance.

## Testing strategy

- Each component (`bitstream`, `huffman`, `lzss`, `threadpool`, `camera`) has a standalone CLI test harness with round-trip/behavioral assertions, built and passing before integration.
- Integration test: full pipeline against a real captured image, compressed size reported, end-to-end round trip verified byte-identical against the original.
- Swift decoder is tested against known-good compressed fixtures generated by the C++ encoder (checked into the repo as test data), so the two implementations are verified compatible without bridging code.

## Milestones (learning checkpoints)

1. `bitstream` — read/write bits, round-trip tested
2. `huffman` — single-threaded, round-trip tested on a file
3. `lzss` — single-threaded, round-trip tested
4. Combine: LZSS → Huffman pipeline, round-trip tested end-to-end
5. `threadpool` — generic, tested with synthetic tasks
6. Parallelize LZSS match search using the thread pool; benchmark vs single-threaded
7. `camera` — gphoto2 subprocess wrapper, tested against physical hardware
8. `server` — single-threaded first (accept → handle → respond), tested with `curl`
9. Wire the thread pool into the server for concurrent connection handling
10. SwiftUI client: HTTP request + Swift decoder implementation, tested against fixtures from step 4
11. Full end-to-end: phone app → Pi → compressed response → decoded image on screen
