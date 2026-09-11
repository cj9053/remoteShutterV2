# How an Image Becomes a Byte Stream (and Why That's All This Codec Sees)

*A teaching guide covering the full LZSS → Huffman pipeline at a conceptual level, and
the practical mechanics of treating an image file as raw bytes. Companion to
`variable-width-codes.md`, which covers Huffman specifically.*

---

## 1. This is not pixel compression — say that out loud until it sticks

Per the design doc (`docs/superpowers/specs/2026-08-27-pi-image-codec-design.md`, lines
12–17): the camera (gphoto2, Canon 60D) already produces a finished JPEG or CR2 file.
**This project never touches pixels, color spaces, or anything image-specific.** The
compressor's entire job is: take the *file* — the exact sequence of bytes already sitting
on disk — and shrink it losslessly, the same way `gzip` would shrink any file, image or
not. That's a deliberate scope decision (line 13–17): no DCT, no quantization, keeps the
project aimed at systems/threading/networking skills rather than image-codec theory.

So "how are image values stored into bits" is really just "how does *any* byte stream
become a bitstream" — there's no special case for images anywhere in this pipeline.

---

## 2. The full pipeline, end to end

```
image file on disk (bytes)
        │
        ▼
   LZSS.encode()          — replaces repeated runs with back-references
        │
        ▼
  token stream            — each token is EITHER a literal byte
                              OR a (offset, length) pair
        │
        ▼
   [not designed yet — see §5]
        │
        ▼
  Huffman.build_frequency_table()   — counts symbol occurrences
        │
        ▼
  Huffman.build_tree() / build_code_table()   — assigns short codes to
                                                  frequent symbols
        │
        ▼
  Huffman.encode()        — for each symbol, BitWriter.write_bits(code, width)
        │
        ▼
  packed bitstream (the actual compressed output, sent over the network)
```

Decompression mirrors this exactly in reverse, and the entire correctness contract is:
**the bytes you get back out the other end are byte-for-byte identical to the original
file.** The SwiftUI client's decoder gets that exact file back and hands it to a normal
image renderer — nothing "pixel-aware" ever happens in your code; the image-ness of the
data is completely incidental to the compressor.

---

## 3. Seeing the bytes yourself, from the terminal

Before writing any code that reads a file, it's worth just *looking* at one, so "a CR2
file is a stream of bytes" stops being an abstract claim:

```bash
xxd path/to/photo.CR2 | head -20
# or
hexdump -C path/to/photo.CR2 | head -20
```

Output looks like:

```
00000000  49 49 2a 00 08 00 00 00  ...   |II*.....|
```

Each pair (`49`, `49`, `2a`, `00`, ...) is one byte, printed in hex, ranging `0x00`–`0xff`
— exactly the range `uint8_t` covers. That first `49 49 2a 00` isn't random: CR2 is
TIFF-based, and `II*\0` (bytes `0x49 0x49 0x2a 0x00`) is the standard TIFF little-endian
magic number, sitting right at the front of the file. You don't need to know anything
about TIFF/CR2 internal structure for this project — you're treating the whole thing as
opaque — but it's a good sanity check that this really is just a flat sequence of numbers
with no visible "pixel" boundary at this level. Pixel data is packed in there somewhere by
the camera's format, but nothing in your pipeline looks for it or cares where it starts.

---

## 4. How the C++ side actually reads it into a `vector<uint8_t>`

```cpp
std::ifstream file(path, std::ios::binary);
std::vector<uint8_t> data(
    (std::istreambuf_iterator<char>(file)),
    std::istreambuf_iterator<char>()
);
```

Two details worth understanding, not just copying:

- **`std::ios::binary` is not optional.** Without it, on some platforms the stream can
  silently translate byte sequences on the way in (historically, newline conversions on
  Windows-style text streams). A JPEG/CR2 file has no concept of "lines" — treating it as
  text and letting the stream "helpfully" transform bytes would silently corrupt the very
  data you're trying to compress losslessly. Binary mode means "give me the bytes exactly
  as they are on disk, no interpretation."
- **Why `uint8_t` and not `char`.** `char` carries a "this is probably text" connotation in
  C++ (it's literally the type string literals use), and on top of that its signedness is
  implementation-defined — a byte value like `0xFF` might read back as `-1` or `255`
  depending on platform, which is exactly the kind of "looks fine, breaks differently on a
  different machine" bug that's miserable to track down. `uint8_t` is explicit: "a number,
  0–255, no text meaning, no signedness ambiguity." This is the same reasoning already
  baked into `bitstream.h`/`huffman.h` — every byte-shaped value in this codebase is
  `uint8_t`, deliberately, never `char`.

This file-reading step isn't one of the numbered components in the design doc's milestone
list — it's plumbing, not an algorithm — but it's the thing that will eventually sit
immediately before `LZSS.encode()` in the real pipeline. Not worth building yet: you're
still isolated-testing Huffman on synthetic byte vectors the test harness constructs
directly, which is the right call (per the design doc's stepwise-refinement philosophy,
line 19–21) — no reason to touch real files until the components that consume them are
proven correct on data you fully control.

---

## 5. The open gap: tokens aren't bytes

Notice step `[not designed yet]` in the pipeline diagram above. `huffman.h`'s
`build_frequency_table` takes a `vector<uint8_t>` — plain bytes. But LZSS's output (design
doc milestone 3) is a stream of **tokens**, and a token is either "one literal byte" *or*
"an `(offset, length)` pair" — which isn't a byte by itself, so it can't go straight into
`build_frequency_table` unmodified.

This is a real, currently-unanswered design question for **milestone 4** (combine LZSS →
Huffman): how do you serialize a token stream into a symbol alphabet Huffman can count and
code? Real-world prior art (DEFLATE) solves it by sharing one alphabet: literal byte
values `0`–`255`, plus extra symbol values `256+` reserved to mean "a match follows — read
length/offset next." Worth knowing this gap exists now, precisely so it *doesn't* need
solving yet — it's explicitly out of scope until Huffman is fully correct standalone and
LZSS exists to actually produce a token stream to serialize.
