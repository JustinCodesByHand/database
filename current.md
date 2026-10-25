# current — the keyword table (worksheet)

Transient working card for the live task — rewritten when the task changes. Logistics (state,
commands, the stage plan) is filled in; the material is blanks. Updated 2026-10-23.

## The task

Make the other 10 keywords tokenize like `SELECT` does — a table + one loop, not ten `if`s.
Only `src/tokenizer.c` changes; only `tokenize_word` is rewritten.

## Where things stand (verified, `script -qec ./build/tests /dev/null`)

- **11 tests, 2 failed** — the correct red starting line, do not touch the tests:
  - `FAIL from_input_is_from_then_eof` → `expected 1, got 24`
  - `FAIL from_case_variant_is_from_then_eof` → `expected 1, got 24`
- Must **stay** green through both stages: `from_prefix_is_ident_then_eof` (`"fromage"`),
  `selection_is_ident_then_eof` (`"selection"`).
- Leak reports on the failing tests are a symptom (the `FAIL` macro `return`s before
  `token_list_free`). Read the FAIL lines, not the leaks.

## The plan — two stages

- **Stage 1:** swap the if/else for the table + loop with ONE table entry → run → must be
  **identical red** (same two, same lines). Identical red = proof the refactor moved nothing.
- **Stage 2:** fill in the other ten → run → `11 tests, 0 failed`.

## Blanks — fill these in

**1 — the pair type** (file-private, above `tokenize_word`):

```c
typedef struct {
  ____ ____;  /* the keyword, as the table spells it */
  ____ ____;  /* the enum value it maps to */
} keyword;
```

**2 — the struct array.** Stage 1 = one entry (you are replacing the hard-coded `if`, nothing
more). Stage 2 = all eleven. Words: `select, from, where, insert, into, values, create,
table, delete, update, set` — each maps to its `TOKEN_*`; look them up in `src/tokenizer.h`.

```c
static const keyword KEYWORDS[] = {
    {____, ____},   /* stage 1: just this one */
    {____, ____},   /* stage 2: eleven lines like this, one per word */
};
```

**3 — `tokenize_word`, the whole scope.** The three field assignments stay verbatim; the
if/else dies. The match is your existing `SELECT` check, generalized — it's still on disk.

```c
static Token tokenize_word(Lexer *lexer, size_t starting_index) {
  Token newToken;
  newToken.start_index = starting_index;
  newToken.word_length = lexer->cursor_position - newToken.start_index;
  newToken.input_string = lexer->input_string + newToken.start_index;

  for (size_t i = 0; i < ____; i++) {
    if (newToken.word_length == ____ &&
        strncasecmp(____, ____, ____) == 0) {
      newToken.token_type = ____;
      ____;  /* entry i said yes — this word IS a keyword */
    }
  }

  ____ /* the word matched NO entry — what is true here, and what must the
          function do about it? Yours to write. */
  return newToken;
}
```

**4 — the landmine, as a question.** `TOKEN_IDENT` can be assigned: (a) before the loop,
(b) after the loop, (c) as the `else` of the match `if` inside the loop. Which placements are
safe? Which one is broken, and what does the broken one do? (One line each, in your head or
on paper — the answer decides what goes where the `____` sits in 3.)

**5 — the count idiom, applied.** `sizeof KEYWORDS / sizeof KEYWORDS[0]`:

- stage 1 (one entry): evaluates to `____` — because `____`
- stage 2 (eleven entries): `____`

## Commands

```sh
make build/tests && timeout 10 ./build/tests   # build + run (hangs are real failures)
script -qec ./build/tests /dev/null            # see FAIL lines (pipes get masked by leaks)
```

## Done when

- **Stage 1 checkpoint:** identical red — `11 tests, 2 failed`, same two, same lines.
  Anything else moves → stop, read, before touching the table.
- **Stage 2:** `11 tests, 0 failed`.
- Then: tick the keyword-table item in `todo`, update `notes/41-tokenizer-state.md`, append
  the session to `notes/90-log.md`, commit `src/tokenizer.c` + `tests/test_tokenizer.c` +
  the notes together.
