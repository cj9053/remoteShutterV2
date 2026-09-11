#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

// How far back a match can point (bytes). Fixed constant for now --
// don't make this configurable until the whole pipeline works end to end.
constexpr size_t WINDOW_SIZE = 4096;

// Shortest match worth encoding as a back-reference instead of literals.
// Below this length, a (offset, length) token costs more than just
// emitting the literal bytes directly.
constexpr size_t MIN_MATCH = 3;

// Longest match a single token can represent.
constexpr size_t MAX_MATCH = 255;

// One position in the token stream: either a single literal byte, or a
// back-reference copying `length` bytes starting `offset` bytes before
// the current output position.
struct Token {
    bool is_literal;

    // Valid when is_literal is true.
    uint8_t literal;

    // Valid when is_literal is false. offset is a distance (>=1),
    // measured backward from the current position in the output being
    // built; length is in [MIN_MATCH, MAX_MATCH].
    uint16_t offset;
    uint16_t length;
};

// A single "how far back / how long" result from searching for a match.
// length == 0 means no match of at least MIN_MATCH was found.
struct Match {
    size_t offset;
    size_t length;
};

// Searches for the longest run of bytes starting at data[pos] that also
// appears somewhere in data[max(0, pos - WINDOW_SIZE), pos). Only
// considers bytes already "seen" (before pos) as match candidates, and
// never reads past data.size() when extending a match forward -- the
// match length is capped at both MAX_MATCH and the remaining bytes in
// data from pos onward.
//
// Ties (multiple candidate offsets giving the same longest length)
// should resolve to *some* deterministic offset -- pick and document
// a rule so encoding is reproducible run-to-run on the same input.
Match find_longest_match(const std::vector<uint8_t>& data, size_t pos);

// Encodes the full byte buffer into a token stream: literal tokens for
// unmatched bytes, (offset, length) tokens for matches of at least
// MIN_MATCH. The very first MIN_MATCH-ish bytes of the input have
// nothing behind them to match against and must be emitted as literals.
std::vector<Token> lzss_encode(const std::vector<uint8_t>& data);

// Replays a token stream back into the original byte buffer: literal
// tokens are appended verbatim, (offset, length) tokens copy `length`
// bytes starting `offset` bytes back *from the output already built*
// -- not from the (nonexistent) input. Copy byte by byte; offset can be
// smaller than length (e.g. "AAAA" encodes as literal 'A' + a match with
// offset=1, length=3), so a block copy of the source region would read
// bytes that haven't been written yet.
std::vector<uint8_t> lzss_decode(const std::vector<Token>& tokens);
