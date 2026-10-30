# current — the road to a finished tokenizer

Supersedes the keyword-table card (done 2026-10-25, `5a9a6d1`). This is the whole remaining
path through Chapter 2, in order. The full exercise prose lives in `tokenizer.md`; this card
is the side-by-side version: for each part — the context to hold, the questions that ARE the
implementation, and the checkpoint that says you may move on.

> **Before you start (verified 2026-10-30):** disk says 12 tests green, keyword table in,
> punctuation NOT yet implemented (unknown chars still silently consumed). If you believe
> exercise 1 was already done, it never hit disk — check unsaved buffers in your editor
> FIRST, before writing on top of it.

## How to run each part

1. Read the part's **Context** — that's what goes in your head.
2. Write the failing test(s) first (one at a time, `make build/tests && timeout 10
   ./build/tests`).
3. Implement — but answer the **Questions** first; writing code for a question you haven't
   answered is how the span bug happened.
4. Checkpoint: new tests green **and** every previous test still green. Then commit.
5. Stuck? Hint bank at the bottom of `tokenizer.md`, section numbered like these parts.
   Stop reading as soon as the next step is clear.

## The invariant you carry through ALL parts

A `Token` is **a span into the source**: `input_string` points at the first byte of the token,
`word_length` says how many bytes belong to it, `start_index` is its offset in the source.
The bytes after the span belong to whatever comes next — or to the whole rest of the source.
Anything that assumes NUL-termination at the token's end (`strlen`, `%s`) is the span bug
waiting to recur. The cursor must always end up exactly one step past the span.

Ask of every new scanner: **which bytes are the token, which bytes does the cursor consume,
and are those the same answer?**

---

## Part 1 — punctuation: `,` `;` `(` `)`

**Context:** `TOKEN_COMMA`, `TOKEN_SEMICOLON`, `TOKEN_LPAREN`, `TOKEN_RPAREN` already exist in
the enum. The `*` branch in `tokenize` is the working template for a one-character token —
read it before writing anything.

**Questions:**
- Four new characters × the same four Token fields — what actually differs between building
  `*` and building `;`? Is the answer four copies of code, or a table + a loop like the one
  you just wrote for keywords?
- Where must these branches sit relative to the silent-skip `else`? What happens to `;` if
  the `else` is checked first?

**Test first:** `";"` → 2 tokens: `TOKEN_SEMICOLON` (pos 0, len 1), EOF. Then each of the
other three. Then `"a,b"` → IDENT, COMMA, IDENT, EOF (4 tokens — count them before running).

**Checkpoint:** all four punctuate; existing 12 stay green. Unknown chars may still be
skipped — that's fixed in Part 7, not here.

## Part 2 — single-char operators: `=` `<` `>`

**Context:** same one-character machinery as Part 1. These are *spelling*: the tokenizer has
no idea what a comparison means.

**Questions:** if Part 1 used a table, do these just… appear in it? If not — why does a
table feel right for 11 keywords + 4 punctuation + 3 operators, but wrong for `*` alone?

**Test first:** `"id=3"` → IDENT, EQ, … wait — `3` isn't a number token yet. So test in
isolation: `"="` → EQ + EOF; then `"<"`, `">"`, and `"a>b"` → IDENT GT IDENT EOF.

**Checkpoint:** six one-character tokens total (`* , ; ( ) = < >` minus still-skipped chars),
nothing else moves.

## Part 3 — two-char operators: `!=` `<=` `>=`

**Context:** the danger is *ordering*: `<` is both a complete token and the prefix of `<=`.
There is no `peek_next` helper yet — but the source is NUL-terminated, and `peek()` past the
end already returns `'\0'`.

**Questions:**
- When you see `<`, what must you inspect **without consuming** to choose between LT and
  LTE? How do you index that byte given what `Lexer` holds?
- What happens if the input is just `"<"` (second byte is `\0`)? Check the branch in your
  head before writing it.
- Which form do you test first — the 2-char or the 1-char? What would "wrong order" do to
  `<=`?
- `!` alone has no enum value. It should reach the Part 7 error path — keep it there.

**Test first:** `"<="` → LTE (len 2!) + EOF. `"<"` → LT + EOF. `">="`, `"!="` likewise. Then
the trap case: `"a<b"` and `"a<=b"` → count and position every token.

**Checkpoint:** the 5-length assert is the one that fails if you consumed one byte when you
should have consumed two.

## Part 4 — identifier spelling: digits and underscores after the first char

**Context:** today a word is `isalpha` only. The guide wants `user_id2` as ONE `TOKEN_IDENT`
— letters/digits/underscore after the first character; the first character itself may not be
a digit. The keyword lookup in `tokenize_word` stays exactly as it is — it receives spans,
not characters.

**Questions:**
- Where in `tokenize` does the word's *first* character get decided, and where does the run
  get consumed? Is the predicate for byte 1 the same as for bytes 2..n?
- `"_name"`: legal by the guide? The guide bans a leading *digit* and says nothing about a
  leading underscore — decide, write it in a comment, test your decision. (Ambiguous spec →
  explicit call is the skill.)
- What does `"2users"` do under your current code, and what *should* it do? (Number scanner
  is Part 5 — note the interaction, don't solve it twice.)

**Test first:** `"user_id2"` → IDENT len 8. `"users"` stays IDENT len 5 (regression). Keyword
behavior unchanged: `"set"` is still SET, `"seti"` still IDENT.

**Checkpoint:** identifiers can now contain `_` and digits-not-at-front; no keyword test moved.

## Part 5 — numbers

**Context:** `TOKEN_NUMBER`. A number is a run of `isdigit`. The tokenizer recognizes the
spelling; converting bytes to a value is NOT this chapter's job — the parser or b-tree layer
will care. Remember the `unsigned char` cast rule for every `<ctype.h>` call.

**Questions:**
- When does the number run stop? (Hint: the answer is the same shape as "when does the word
  run stop".)
- Where in the dispatch does a digit-led token branch go, and what must it decide before
  Part 4's identifier rule can conflict with it?
- `"2users"` — your decision from Part 4's note, now made real. NUMBER(2) then IDENT(users)?
  Or error? Pick, comment, test.

**Test first:** `"42"` → NUMBER (pos 0, len 2) + EOF. `"id=42;"` → IDENT EQ NUMBER SEMI EOF —
the first test that uses Parts 1–5 together. Assert all five positions.

## Part 6 — quoted strings `'…'`

**Context:** `TOKEN_STRING`. The guide: content span **excludes the quotes**; unterminated →
error with the **opening quote's** position. Whitespace inside quotes is content — the outer
whitespace skip only runs *between* tokens, so once you're inside the quote that's already
true; make sure your scanner doesn't re-skip.

**Questions:**
- The token's `input_string` must point at starting_index **+1** and `start_index` too, with
  `word_length = end - start - 2`. Which loop finds the closing quote, and what does it do
  when it walks into `\0`?
- The escape rule: the guide gives you none. Add one? (No — record the choice; ask before
  inventing grammar.)

**Test first:** `"'hello'"` → STRING (pos 1, len 5) + EOF — the first token whose
`start_index` is not where the branch began. Then `"'a b'"` (space inside). Then unterminated
`"'abc` → your agreed error behavior (Part 7's contract may be needed here — if so, stub
`had_error`/`error_pos` and say so in the commit).

## Part 7 — unknown characters become errors

**Context:** this deletes the silent-skip `else` — the stopgap from October. `TokenList` has
`error_msg`, `error_pos`, `had_error`. **This part begins with a decision, not code:**

**The design question (write the answer down before the test):** when a character is
unspellable, does `tokenize`
(a) return the list with `had_error = true`, `error_pos` at the bad char, `error_msg` naming
it — caller frees normally; or
(b) free everything internally and return `NULL`?
Which one lets the REPL show `"near character '@' at position 6"` later? Which one matches
what the OOM path already half-does (look at the `errorrrr` branch — it returns NULL *and*
leaks)? Decide the contract ONCE, for both error kinds.

**Test first:** `"SELECT@"` → per your contract. If (a): list non-NULL, `had_error` true,
`error_pos` 6. Also decide + test: stop at first error, or skip and continue?

**Checkpoint:** no input can be silently mangled anymore; every byte is consumed into a
token, an error, or whitespace. That's the real definition of a complete scanner.

## Part 8 — EOF convention + list robustness

**Context:** known defects, now worth fixing (see `notes/41`, defects 2/3/6): EOF's
`input_string` points at the source *start* while `start_index` is the *end* — inconsistent.
`tokenize` ignores `list_push`'s return (word and `*` pushes), so a grow failure is invisible
until the EOF push trips it.

**Questions:** what should EOF's `input_string` be so that "the span is empty and it sits at
the end" is the whole truth? (One line of `tokenize` changes — then find which existing test
asserts the old lie… none do. That's why you add one.)

**Tests:** the EOF convention (pos and len); `token_list_free(NULL)` doesn't crash (read the
function first — is it already safe? test it anyway); a 10,000-`"a "`-repeated input tokenizes
to 10,001 tokens under ASan with no leak — the stress test that proves the growth logic.

## Part 9 — `token_type_name`

**Context:** the body already exists: `scratch_type_name` in `tests/scratch.c` — search by
*shape*, not name. It becomes `tokenizer.c`'s, declared in the header (the comment explaining
why it was disabled must go with it).

**Questions:** display names (`"*"`) or enum names (`"TOKEN_STAR"`)? The guide builds `expect`
messages from it later — read what error text you want in Part 7's REPL and let that answer
decide. Then: what does the `default` case return for a bogus int? (`-Wswitch` only warns on
enum switches — do you have an enum switch?)

**Test:** at least one real assertion per… no — one or two (SELECT, EOF, error fallback); the
value is the compiler-checked list of cases, not re-typing 27 asserts.

## Part 10 — the end-of-chapter checkpoint

**The one test that IS the chapter:** `"SELECT * FROM users WHERE id = 3;"` →
`SELECT, STAR, FROM, IDENT(users), WHERE, IDENT(id), EQ, NUMBER(3), SEMICOLON, EOF` — ten
tokens, assert at least the types, the length, and three of the ten positions.

Then the philosophical one: `"FROM SELECT , , ;"` tokenizes **without complaint**. If it
errors, your tokenizer is trying to be a parser.

Then the guide's "Done when" (`src/minidb-c-guide.md` ~line 2658): every statement form
tokenizes, every error case has a position, spelling-only, no leaks. That's the chapter end.

---

## After EVERY part (the ritual — `AGENTS.md` §8)

- `todo`: tick or restate what moved.
- `notes/41-tokenizer-state.md`: what the lexer handles, defects closed/added.
- `notes/90-log.md`: one dated paragraph — what you did, what surprised you.
- Commit code + tests + notes together, push (`P` or let the hook).

## Commands

```sh
make build/tests && timeout 10 ./build/tests   # build + run (timeout catches a hang)
script -qec ./build/tests /dev/null            # see FAIL lines past ASan noise
make scratch ARG='"SELECT * FROM users;"'       # watch tokens get built, live
```

If a part's questions make you uncomfortable — i.e. you can't guess the shape of the answer —
that's the signal to open `tokenizer.md`'s hint bank for that section, not to skip ahead.
