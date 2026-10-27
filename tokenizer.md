# Tokenizer exercises — from characters to tokens

An ordered exercise path for completing Chapter 2. The style takes inspiration from
[Ziglings](https://codeberg.org/ziglings/exercises/) (small tasks, nearby nudges) and
[50 small C exercises](https://github.com/LilTaiga/50-small-c-exercises) (clear contracts,
examples, and a run command). This is written for **this repo's code and tests**, not copied
from either project.

## How to use this file

Work **one checkpoint at a time**. Read its goal and examples; write its tests; implement only
that checkpoint; run the whole suite before continuing. The hint bank is at the bottom—try
the task first, then reveal only as much of the hint as you need. Nothing here is a solution
listing.

Keep the live build/test state in `current.md`; keep the overall project queue in `todo`.
For the design contract and hard-earned string-span lessons, see `notes/40-tokenizer-contract.md`.
For current implementation defects, see `notes/41-tokenizer-state.md`.

## The mission

Turn the characters in a source string into a sequence that a later parser can inspect. The
tokenizer recognizes **spelling**, not whether SQL is grammatically sensible. It should be
able to report tokens for something nonsensical like `FROM SELECT , , ;`; deciding whether
that sequence is valid belongs to the parser.

The guide's target example is:

```text
SELECT * FROM users WHERE id = 3;
```

Eventually, the sequence should describe the keyword, operator, identifiers, number,
punctuation, and end of input separately.

## What is already built

Verified on `main` on 2026-10-26:

- `tokenize` skips whitespace, scans alphabetic words, recognizes all 11 SQL keywords through
  a static table, recognizes `*`, and appends EOF.
- Keyword matching is case-insensitive and checks the whole word, not a prefix.
- Tokens are spans into the caller's source: pointer + length + starting index. They do not
  own or copy the text; the source must outlive the token list.
- **12 tests pass.** They cover empty/whitespace input, `SELECT`, `*`, `FROM`, case and prefix
  matching, and `SELECT * FROM users` including token positions.
- The current fallback **silently consumes unknown characters**. This prevents hangs, but is
  temporary—not correct final error handling. `, ; ( ) = < >`, digits, and quotes are not
  implemented as tokens yet.

Before each task, verify the code and test count on disk. The editor has previously written a
stale buffer over newer test code.

## The invariant to carry through every exercise

`Token.input_string` points at the first byte of a token **inside** the original source; it is
not guaranteed to be NUL-terminated there. `Token.word_length` says exactly how many bytes
belong to that token. `start_index` is the token's position in the source, and the lexer cursor
points at the next byte to inspect (the end of a scanned span is exclusive).

Whenever a token is built, ask yourself: **which bytes does this token own as its span, and
has the cursor consumed exactly those bytes?** Keep the source alive; do not print a span as a
plain `%s` string.

## Exercise 1 — punctuation tokens

**Goal:** teach the main loop to recognize `,`, `;`, `(`, and `)` as real one-character
tokens instead of silently dropping them.

Start with one test, such as `";"` → `TOKEN_SEMICOLON, TOKEN_EOF`. Assert the token type,
length `1`, and start position `0`. Then cover the other punctuation and one mixed input. The
existing enum already has `TOKEN_COMMA`, `TOKEN_SEMICOLON`, `TOKEN_LPAREN`, and
`TOKEN_RPAREN`.

**Keep in mind:** a one-character token's span is one byte. The character is consumed exactly
once; otherwise the outer loop can revisit it forever. Follow the existing `*` path rather
than rewriting word scanning.

**Checkpoint:** all punctuation tests pass; the existing 12 remain green. Do not make `*`
change behavior.

## Exercise 2 — one-character comparison operators

**Goal:** recognize `=`, `<`, and `>` as `TOKEN_EQ`, `TOKEN_LT`, and `TOKEN_GT`.

Test each alone and in a short sequence between words. Include positions so that a correct
type at a wrong offset cannot pass unnoticed.

**Keep in mind:** dispatch is based on the current character. This is still token spelling,
not a comparison expression: the tokenizer does not decide what either side means.

**Checkpoint:** all three operators have the right type, one-byte span, and position; previous
tests stay green.

## Exercise 3 — two-character operators

**Goal:** recognize `!=`, `<=`, and `>=` as `TOKEN_NEQ`, `TOKEN_LTE`, and `TOKEN_GTE`.

Write paired cases: the two-character form is one token of length `2`; the one-character
form remains its own token. Check that the characters immediately before and after an operator
are not swallowed.

**Think about:** when the current character could begin either a one- or two-character token,
which form must be checked first? What should happen to the cursor if the second character is
not present or does not match?

Do not invent a token for a spelling absent from the enum. In particular, `!` by itself needs
the error behavior from Exercise 7.

## Exercise 4 — identifier spelling

**Goal:** allow digits and underscores **after** an identifier's first character, as required
by the guide: `user_id2` is one `TOKEN_IDENT`; a name beginning with a digit is not an
identifier.

Add tests that contrast a valid start (`user_id2`) with a digit at the start (`2users`). Keep
keyword tests too: the scanner must consume the *whole* identifier before asking the keyword
table whether it matches.

**Think about:** are the rules for the first character and later characters identical? If not,
which predicate belongs at the start, and which predicate belongs while continuing?

The guide names letters, digits, and underscores. Do not broaden this to Unicode or
database-specific quoted identifiers without deciding that separately.

## Exercise 5 — integer numbers

**Goal:** scan a run of decimal digits as one `TOKEN_NUMBER`, with a span over exactly those
digits.

Start with `"42"` → NUMBER + EOF; then test a number in a sequence and its `start_index` and
`word_length`. Try adjacent boundaries such as `id=42;` as the earlier exercises become
available.

**Keep in mind:** `isdigit` is from `<ctype.h>`; pass it an `unsigned char` value, not a
possibly negative plain `char`. The current enum has `TOKEN_NUMBER`, but no arithmetic or
signed-number syntax is requested here. The tokenizer recognizes the digit spelling; it does
not convert the bytes to an integer value.

## Exercise 6 — quoted strings

**Goal:** scan the guide's single-quoted form, for example `"'hello'"`, as one
`TOKEN_STRING`. The token's text span excludes the opening and closing quote characters.

Test the content and its location, including a string with spaces so whitespace inside the
quotes is not treated as a separator. Test an unterminated quote separately: the guide expects
an error and the opening quote's position.

**Scope question:** the guide specifies quoted strings but does not specify escape syntax.
Do not silently invent backslash or doubled-quote rules; ask before extending the grammar.

## Exercise 7 — unknown-character errors

**Goal:** replace the temporary “consume and ignore” fallback with an error for unsupported
characters. The guide's example is `"SELECT@"`: report the offending character and a usable
position.

Before implementation, settle the error contract: when a lexical error occurs, does
`tokenize` return the partial list with `had_error` set, or return `NULL`? The existing
`TokenList` has `had_error`, `error_msg`, and `error_pos`, but the current OOM path also
returns `NULL` and has cleanup problems. Decide the observable behavior **before** writing the
assertions; do not conflate malformed input with allocation failure.

Write a test for the agreed result, then test an error after a valid token as well. Decide
whether scanning stops at the first error or continues; make the test match that decision.

## Exercise 8 — finish the list and EOF contract

The dynamic list already grows, but its failure path is unfinished. Revisit it after token
scanning is stable:

- Check every `list_push` result; a failed grow must not be followed by a write through an
  invalid buffer.
- Decide how `grow_list_capisity` communicates failure and how the caller frees a partial list.
- Resolve the EOF span: today EOF points `input_string` at the source start while
  `start_index` is at the end. Choose and test a consistent convention; EOF has length zero.
- Add `token_list_free(NULL)` coverage and a 10,000-token stress test. The latter exercises
  repeated growth; ASan/UBSan should remain clean.

Allocation failure is hard to trigger deterministically. Keep the ownership and failure
paths simple enough to inspect; do not claim an OOM test you have not actually run.

## Exercise 9 — token names

**Goal:** implement `token_type_name` for diagnostics/tests, using the recoverable
`scratch_type_name` mapping in `tests/scratch.c` as a reference.

First settle what the function returns: enum-style labels (`"TOKEN_STAR"`) or display labels
(`"*"`). The guide only requires useful names for errors; choose one consistent contract,
uncomment/add the public declaration, and test every enum value including the error value.
Unknown enum values should have an intentional fallback.

## Exercise 10 — full tokenizer checkpoint

Use the guide's full SQL-shaped input:

```text
SELECT * FROM users WHERE id = 3;
```

Assert the entire token sequence, each important span and position, and exactly one EOF.
Also test a spelling sequence that is not valid SQL: `FROM SELECT , , ;` should still
tokenize. No parser rules here.

Run the entire suite, not only the newest test. Build flags are strict (`-Werror`) and
ASan/UBSan are enabled by the Makefile.

## Work loop

For **each** exercise:

1. Read the current implementation and the relevant enum/struct contract first.
2. Add one focused test that describes the observable behavior.
3. Run `make test` and inspect the actual failure (use a timeout if testing a possible hang).
4. Change only the scanner path needed for that checkpoint.
5. Run `make test` again; confirm previous cases remain green.
6. If stuck, use the hint bank below from top to bottom. Stop as soon as the next step is clear.

Useful commands:

```sh
make test                         # build + full test suite
make build/scratch                # build standalone tokenizer harness
make scratch ARG='"SELECT * FROM users;"'  # bounded scratch run
script -qec ./build/tests /dev/null       # preserve FAIL output if ASan exits early
```

## Hint bank — open only after trying the exercise

### 1. Punctuation

- **Nudge:** What is the smallest observable unit that distinguishes `;` from whitespace?
- **More:** Compare this path with the already-working `*` path. What must each path do to
  the cursor before the next outer-loop iteration?
- **Context:** A small `switch` on `peek()` fits a fixed set of single characters. The token
  still needs a pointer into the source, a length, and a start index.

### 2. One-character operators

- **Nudge:** The enum already names their results. Which input byte selects each one?
- **More:** Can you test the operator between words and assert the positions of all three
  tokens?

### 3. Two-character operators

- **Nudge:** The first byte is shared by a short and long spelling. Which one would be lost if
  you consume the short form too early?
- **More:** Check the following byte without consuming it; consume it only when it completes
  the longer spelling. The end-of-input byte is `\0`, not a valid second operator byte.

### 4. Identifier spelling

- **Nudge:** The first character has stricter rules than the rest.
- **More:** Keep the initial test separate from the “continue scanning” test. After scanning,
  pass the complete span to the keyword table that already works.

### 5. Numbers

- **Nudge:** A number token is still a span; its contents are not converted here.
- **More:** Find the first digit, then determine where the consecutive run ends. Test an
  identifier boundary immediately after the digits.

### 6. Strings

- **Nudge:** There are three positions to distinguish: opening quote, first content byte,
  closing quote.
- **More:** Keep scanning through spaces until the closing quote or `\0`. Compare the token's
  span with the source span: should either quote be included?
- **Error case:** Remember the opening quote's index while scanning. At `\0`, that is the
  position the guide asks you to report.

### 7. Unknown characters

- **Nudge:** Recognized paths must get first chance; the fallback is for what remains.
- **More:** The loop must not silently lose a byte. Record the first offending byte and index
  using the contract you chose, then make the return/partial-list behavior consistent.

### 8. List / EOF / stress

- **Nudge:** On `realloc` failure, the original allocation is still alive. What must not be
  overwritten before success is known?
- **More:** Check the push result at every call site. Where is the one place that can release
  both the list buffer and the list object if tokenization cannot continue?
- **EOF:** A zero-length span has a pointer and a position. State your convention, then test
  it; don't rely on an unexamined initializer.

### 9. Token names

- **Nudge:** `tests/scratch.c` already contains a switch-shaped mapping. Search for
  `scratch_type_name`, not only for the intended function name.
- **More:** Enumerate every value from `tokenizer.h`; compile with `-Wswitch` awareness, then
  test the returned string for each one.

## References

- Project specification: `src/minidb-c-guide.md`, Chapter 2 (starts near line 2468); test list
  near 2610; troubleshooting near 4693.
- Token/source lifetime and span invariant: `notes/40-tokenizer-contract.md`.
- Current implementation and known defects: `notes/41-tokenizer-state.md`.
- Exercise-style inspiration: [Ziglings](https://codeberg.org/ziglings/exercises/) and
  [50 small C exercises](https://github.com/LilTaiga/50-small-c-exercises).
