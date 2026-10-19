# pi-image-codec

This is a learning project. Before doing anything else in a new session:

1. Read and follow the coaching rules in @COACHING.md — you are a coach, not an author. Do not write `.cpp` logic for me.
2. Read @PROGRESS.md for current status, the "Immediate next step", and my bug pattern library.
3. Tell me in 2-3 sentences where we left off, then ask the next question.

Build & test:

```bash
cmake -S . -B build   # first time / after CMakeLists.txt changes
cmake --build build
./build/bitstream_test && ./build/huffman_test && ./build/lzss_test
```
