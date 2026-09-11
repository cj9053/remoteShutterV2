# Variable-Width Codes — Why Huffman Coding Works

*A teaching guide for the `huffman` component of pi-image-codec. Written to explain the
reasoning, not just the recipe — read this once, then it should make the code in
`build_tree`/`build_code_table` feel inevitable rather than memorized.*

---

## 1. The problem fixed-width codes leave on the table

Every byte in your input is one of 256 possible values. The "obvious" encoding is: every
symbol gets 8 bits, no exceptions. That's what you're already storing on disk — it's why
a 1000-byte file takes 1000 bytes.

But look at real data. In English text, `e` shows up constantly and `q` almost never. In
a photo, most pixels are close to their neighbor's value, so small delta values dominate
and large ones are rare. **Fixed-width coding spends the same 8 bits on `e` as it does on
`q`, even though `e` carries less information per occurrence** — you could almost predict
it's coming.

This is the core idea compression is built on:

> **A symbol you can predict is a symbol that costs less to transmit.**

Frequent symbols are predictable (in the sense that a `y` occurring often makes learning
"the next symbol is `y`" a good bet). Information theory (Shannon) formalizes this: the
information content of a symbol with probability `p` is `-log2(p)` bits. High-probability
symbols → few bits of *actual information* → should cost few bits to encode. Rare symbols
→ many bits of information → fine if they cost more bits to encode, because they're rare.

That's the entire justification for variable-width codes: **assign short codes to
frequent symbols and long codes to rare symbols**, and the *average* bits-per-symbol drops
below 8, even though some symbols individually get *more* than 8 bits worth of code.

---

## 2. The catch: how do you know where one code ends and the next begins?

Suppose you try the "obvious" variable-width scheme:

| symbol | code |
|---|---|
| A | `0` |
| B | `1` |
| C | `01` |

Encode `"AB"` → `0` + `1` = `"01"`.
Encode `"C"` → `"01"`.

**Both inputs produce the identical bitstream.** There's no way to tell, on decode,
whether `01` means `"AB"` or `"C"`. Fixed-width codes never have this problem — you always
know the next symbol starts exactly 8 bits later. The moment codes have different
lengths, you've introduced ambiguity, *unless* you're careful about which codes you allow.

### The fix: prefix-free codes

A code is **prefix-free** (a.k.a. a *prefix code*) if **no code is a prefix of any other
code**. Fix the table above:

| symbol | code |
|---|---|
| A | `0` |
| B | `10` |
| C | `11` |

Now `01` doesn't parse as "prefix of anything else" — decoding `"AB"` = `0` + `10` =
`"010"`, and decoding `"C"` = `"11"`. Nothing collides, because once you match `0`, you
know immediately it was `A` (no other code starts with `0`), and once you match `1` you
know you need one more bit to disambiguate `B` vs `C`.

**Prefix-free codes can be decoded greedily, bit by bit, with zero lookahead and no
delimiters between symbols.** This is *why* Huffman coding builds a binary tree: a binary
tree where symbols only live at **leaves** *automatically* produces a prefix-free code.
Walk root → leaf, emit `0` for left / `1` for right; because leaves have no children, no
leaf's path can be a prefix of another leaf's path (if it were, the shorter path would
have to pass *through* a leaf on its way to the longer one, but leaves have no outgoing
edges to continue the walk).

This is the connection between "binary tree" and "prefix code" that makes the whole
algorithm click: **you're not choosing bit patterns directly — you're choosing a tree
shape, and the tree shape guarantees the prefix property for free.**

---

## 3. Why *this* tree shape is optimal (the greedy merge)

Given the tree ⟺ prefix-code connection, the remaining question is: which tree minimizes
the *average* code length, weighted by frequency? A symbol's code length equals its
**depth** in the tree. So we want frequent symbols shallow (short codes) and rare symbols
deep (long codes) — cost of a symbol = `frequency × depth`, and we want to minimize the
sum of that over all symbols.

Huffman's insight, stated as a greedy rule:

> **Repeatedly take the two lowest-frequency nodes and merge them under a new parent.**
> The parent's frequency is their sum. Repeat until one node (the root) remains.

Why does this work? Two intuitions:

- **The two rarest symbols should end up as deep — and as close to each other in
  depth — as any two symbols in the tree.** They contribute the least "weight" per unit
  of depth, so it's "cheapest" to push them to the bottom. Merging them first guarantees
  they end up siblings at the deepest point the algorithm creates.
- **Once merged, the pair behaves as a single combined symbol for the rest of the
  algorithm** (that's why the parent's frequency is the *sum* — from here on, the
  algorithm can't tell "this node" apart from "an actual input symbol with this
  frequency"). This is what makes it a valid *greedy* algorithm rather than a heuristic:
  the subproblem after merging is the exact same kind of problem, one size smaller,
  and it can be proven (by exchange argument / induction) that this greedy choice never
  prevents the global optimum. That's why `build_tree` doesn't need backtracking — pop
  two, merge, push, repeat is provably sufficient.

This is exactly `build_tree`: the `priority_queue<..., CompareNodes>` with `>` in the
comparator gives a **min-heap** (smallest frequency at `top()`), so `top()`/`pop()` twice
always gets you "the two lowest-frequency nodes remaining" — including nodes that were
themselves created by earlier merges, which is what lets combined subtrees keep
participating correctly.

### Worked example: `"ABRACADABRA"`

Frequencies: `A:5, B:2, R:2, C:1, D:1`.

```
Step 1 (lowest two): C(1), D(1) → merge → CD(2)
Step 2 (lowest two): B(2), R(2) → merge → BR(4)   [CD(2) also qualifies — ties broken
                                                     by whichever order the heap pops;
                                                     either choice is optimal]
Step 3 (lowest two): CD(2), BR(4) → merge → CDBR(6)
Step 4 (lowest two): A(5), CDBR(6) → merge → root(11)
```

One possible resulting tree:

```
                root(11)
               /        \
             A(5)      CDBR(6)
                       /      \
                    CD(2)    BR(4)
                   /    \    /    \
                 C(1)  D(1) B(2) R(2)
```

Reading codes as root→leaf (`0`=left, `1`=right):

| symbol | freq | depth | code |
|---|---|---|---|
| A | 5 | 1 | `0` |
| C | 1 | 4 | `1000` |
| D | 1 | 4 | `1001` |
| B | 2 | 3 | `101` |
| R | 2 | 3 | `110` |

Notice the property this guarantees: **the most frequent symbol (`A`, freq 5) got the
shortest code (1 bit)**, and the two rarest (`C`, `D`, freq 1 each) got tied for the
*longest* codes (4 bits) — exactly the shape we wanted in §1, produced mechanically by
the merge order, with no manual reasoning about "which symbol deserves how many bits."

Compare total cost: fixed-width would cost `11 symbols × 8 bits = 88 bits`. This tree
costs `5(1) + 2(3) + 2(3) + 1(4) + 1(4) = 5+6+6+4+4 = 25 bits` for the same 11 symbols —
before even packing, that's the win variable-width coding buys you.

---

## 4. From tree to code table: turning the walk into data

`build_tree` gives you the *shape*. `build_code_table` has to actually **walk it and
record the path**. The recursion is naturally: *carry the accumulated path down as you
descend, and only write an entry out when you hit a leaf* (internal nodes aren't
symbols — they don't get a code of their own, they're just structure).

State you need to thread through the recursion:
- the current node,
- the bits accumulated so far (as a growing integer — shift left, OR in 0 or 1 per step,
  exactly the "insert bit" pattern from `bitstream`'s `read_bits`),
- the depth so far (this becomes the code's *width* — you can't just store the bit
  pattern, because `0b101` and `0b0101` are the same integer but different codes; width is
  what disambiguates, which is why the contract is `pair<uint32_t code, int width>` and
  not `uint32_t` alone).

The one-symbol edge case (from `build_tree`'s doc comment) matters here too: if the input
only ever contains one distinct byte, the "tree" is a single leaf with no merges at all —
there's no root→leaf edge to generate a `0`/`1` from. The convention mentioned in
`PROGRESS.md` (give it code `0`, width `1`) is a deliberate special case you have to
handle explicitly in `build_code_table`, or you'll walk a root that's *already* a leaf and
emit a zero-width code, which `huffman_encode` can't pack (0 bits written = decoder can
never make progress).

---

## 5. Why this needed `bitstream` built first

A code table entry like `A → (code=0b1001, width=4)` is not "a byte." Codes are usually
1–15ish bits, never byte-aligned, and get packed back-to-back with zero padding between
symbols. That's a fundamentally different I/O pattern than "write this byte" — it's *why*
milestone 1 (`BitWriter`/`BitReader`, MSB-first bit packing) had to exist before Huffman
could be implemented at all. `huffman_encode` is really just: *for each input byte, look
up its `(code, width)`, and `write_bits(code, width)`.* `huffman_decode` is the mirror:
*read one bit at a time, walk left/right from `root`, and whenever you land on a leaf,
emit that byte and reset back to `root` for the next symbol* — which is also exactly why
decoding needs the *tree*, not just the code table: a table alone doesn't tell you "have I
read a complete code yet," but a tree walk does (you know you're done exactly when you
hit a leaf).

---

## 6. Things that bite people later (worth knowing now)

- **Skewed frequency distributions can make codes long.** In the worst case (frequencies
  following a Fibonacci-like sequence), Huffman codes can be as long as `n-1` bits for `n`
  symbols. With ≤256 possible byte values, the theoretical max width is 255 bits — which
  is exactly why the contract stores `uint32_t code`, capping you at 32 bits, and is a
  latent constraint worth remembering if a test ever mysteriously truncates a code on
  wildly non-uniform input.
- **Ties in the priority queue** (two nodes with equal frequency) are resolved arbitrarily
  by whatever order `std::priority_queue` happens to pop them in — this is fine (any tie
  resolution produces an equally-optimal tree in terms of total bit cost), but it does
  mean **the exact tree shape, and therefore exact code values, aren't uniquely
  determined** by the frequency table alone. Two different (correct) implementations can
  legally produce different bitstreams for the same input. This is also why
  `serialize_tree`/`deserialize_tree` (deferred per `PROGRESS.md`) will eventually matter:
  a real wire format has to ship *this exact tree*, not just trust the decoder to
  regenerate an "equivalent" one from frequencies, because frequencies alone
  under-determine the tree when there are ties.
- **Prefix-free ⇒ no separators needed, but also ⇒ you must know when to stop.** That's
  the entire reason `huffman_decode`'s contract takes `original_length` — the bitstream
  itself has no "end of symbols" marker (padding bits at the end of the last byte would
  otherwise be misread as more codes), so the decoder has to be told externally when to
  stop consuming.
