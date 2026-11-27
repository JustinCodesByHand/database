# 41 — Tokenizer state & defects (`src/tokenizer.c`)

Part of the [notes tree](../ainotes.md). Read before working on the tokenizer.

## Status (verified on disk 2026-10-25, branch `main`)

**Build is green. 12 tests, 0 failed.**

repl (3) + tokenizer: empty, whitespace-only, `SELECT`, `*`, `FROM`, `fRoM`,
`fromage`→IDENT, `selection`→IDENT, **`"SELECT * FROM users"` → SELECT/STAR/FROM/IDENT/EOF**
(types **and** positions — the multi-word guard).

> **Corrections to earlier notes.** The alpha-loop hang is fixed; the **prefix-match bug is
> fixed and committed** (`word_length == strlen("SELECT") && strncasecmp(...)`); and the
> infinite loop on *unrecognised* characters is gone too (they are now consumed — see below).

## What the lexer handles now

Whitespace (skipped), alphabetic words, `*`, and EOF — plus **all 11 keywords**, recognized
by the `static const keyword KEYWORDS[]` table + one loop in `tokenize_word` (length check +
`strncasecmp`; the third argument is **`word_length` — the span**, never
`strlen(input_string)`; the debugger recipe for watching that lives in
[40-tokenizer-contract](40-tokenizer-contract.md)):

- `SELECT * FROM users` → `SELECT, STAR, FROM, IDENT(users), EOF` — pinned by test.
- Case-insensitive (`fRoM` → FROM); prefix-safe (`fromage`, `selection` → IDENT).
- **Unknown characters** (`,` `;` `(` digits `@` …) are still consumed and silently skipped
  (deliberate stopgap — no tokens or errors yet).

## Functions present

| Function | Notes |
|---|---|
| `peek(const Lexer *)` | returns `unsigned char` |
| `is_at_end(const Lexer *)` | true iff `peek == '\0'` |
| `increment_cursor(Lexer *)` | `static`, returns `void` (guide's `advance` wanted `char`) |
| `grow_list_capisity(TokenList *)` | owns the capacity math incl. the 0-guard; returns `void` |
| `add_tkn_to_tknlist(TokenList *, Token)` | copies the 4 token fields verbatim |
| `list_push(TokenList *, Token)` | grow-then-push; returns `false` if `had_error` |
| `token_list_free(TokenList *)` | NULL-safe, silent |
| `tokenize_word(Lexer *, size_t)` | walks the KEYWORDS table; `TOKEN_IDENT` default **before** the loop |
| `tokenize(const char *)` | main loop: whitespace → word / `*` / skip-unknown |

## Open defects

1. ~~Only `"select"` is a keyword~~ **CLOSED (2026-10-25):** all 11 keywords live in the
   `static const` table (`7165518`); the `strlen(input_string)` span bug found in review was
   fixed (`word_length`) and guarded by `select_from_users_full_statement`.
2. **`list_push`'s return value is ignored** every time it is called in `tokenize` (both the
   word push and the `*` push). A grow failure is not seen. (The EOF push *does* check it.)
3. **`grow_list_capisity` returns `void`.** Failure travels as a side effect (`had_error`)
   rather than a return value.
4. **`had_error` is sticky and overloaded.** Once true, `list_push` returns `false` for every
   later push. It is also the struct's *general* error flag (paired with
   `error_msg`/`error_pos`), so a caller cannot distinguish "malformed SQL" from "out of
   memory". Still an open design decision.
5. **`token_type_name` is not implemented.** Declaration commented out in the header. See
   [42-token-type-name](42-token-type-name.md).
6. **EOF token fields are inconsistent.** `{TOKEN_EOF, lexer.input_string, 0,
   lexer.cursor_position}` sets `token_string` to the *start* of input but `source_index` to the
   *end*. `token_length = 0` is correct. Natural value: `lexer.input_string +
   lexer.cursor_position`. (Field names updated 2026-11-27.)
7. **Unknown characters are silently dropped** (the `else` in `tokenize`). Fine as a hang-guard,
   but there is no error reported yet — the guide wants e.g. `"SELECT@"` to name the character.
   Also blocks `,` `;` `(` `)` and all comparison operators, which the chapter needs next.
8. **Single-character tokens are hand-rolled.** `*` is built inline
   (`{TOKEN_STAR, input + start, 1, start}`). That is fine for one operator; the other operators
   will want a small table or a `switch` on `peek`.
9. **Stale artifact:** an `aaaaa` fragment in the `tokenize` doc comment.

**Absent helpers the guide lists:** `peek_next`, `match`, `scan_number`, `scan_identifier`,
`scan_string`.

## Work in progress — punctuation (Part 1 of `tokenizer.md`)

**Started 2026-10-31.** The user is implementing punctuation recognition (`*`, `;` so far).
Changes are uncommitted.

### Current structure on disk

- `PUNCTUATIONS[]` table: `{"*", TOKEN_STAR}`, `{":", TOKEN_SEMICOLON}` — note the second
  entry's symbol vs. the guide's `;`.
- `tokenize_punctuation(Lexer *, size_t)` — scans the table, uses `strncasecmp`.
- Main loop: `switch (peek(&lexer))` handles `*` and `;`, with a `made_punct` flag
  gating `tkn_list_add`. Unknown chars fall through to `default` (consume one, no push).
- `tkn_list_add` (renamed from `list_push`) sets `had_error` on grow failure; the
  `tokenize_punctuation` path also sets `had_error` when no table entry matches.

### Debugging observations (user's own)

- The `input_string` pointer for the new punctuation token correctly points at the
  punctuation character (e.g. `*`), with the rest of the source following it — this is
  correct span behavior.
- The function takes the `had_error` exit route (returns NULL), causing the test to fail
  and leaking memory on the error path.
- The user identified that **the table comparison is the culprit**.
- The user asked: "what do you think about using the `had_error` flag to determine whether
  I push the punctuation?" — see the design discussion in the 2026-10-31 session notes.

### Design questions still open

1. Should `had_error` gate pushing punctuation tokens? (Recommendation: no — use a local
   flag; `had_error` is sticky and conflates "an error occurred" with "this token is
   invalid". See the notes above.)
2. The `PUNCTUATIONS` table symbols vs. what `strncasecmp` compares against — is the
   comparison starting from the right offset?
3. The `:` vs `;` entry — intentional or typo?

### To resume

Set a breakpoint at `tokenize_punctuation` entry. Watch `starting_index`,
`lexer->cursor_position`, and what `strncasecmp`'s first argument points at. Compare with
`new_punctuation_token.input_string`. The mismatch between those two pointers is where the
bug lives.

## Current verified state (2026-11-26)

Parts 1-5 are committed and green: **33 tests, 0 failed** before Part 6 tests were added.
Punctuation/operators use a giant `switch` in `tokenize` with a local `made_punct` push gate
(not `had_error`); two-char operators use direct lookahead at `cursor_position + 1`.
Identifiers accept `isalpha || isdigit || '_'` after byte 1. Numbers have an
`else if (isdigit(...))` branch before the switch and `tokenize_number` returns
`TOKEN_NUMBER`.

## Part 6 tests (added 2026-11-26, all red)

Five tests in `tests/test_tokenizer.c`: quote-excluded string span, interior whitespace,
empty string, trailing bytes after the close, unterminated string error. **38 tests, 5
failed; the previous 33 stay green.** Scratch proof: `make scratch ARG="\"'hello'\""`
currently yields `IDENT pos=1 len=5` — the two skipped quotes are handled by `default:`,
so the span is already right and only the token type is wrong. The unterminated test assumes
the partial-list + `had_error` contract — decision now confirmed below.

## Error contract — DECIDED 2026-11-26

Learner chose **(a) partial list**: on any error (lexical or OOM) `tokenize` returns the list
with `had_error = true`, `error_pos` at the offending byte, `error_msg` set; the caller frees
normally. Stated reason: *"so I know where the error exists"* — a `NULL` return destroys the
position. The `errorrrr` / `return NULL` tail of `tokenize` is obsolete and must be rewritten.

**Immediate consequence:** `tkn_list_add` refuses every push once `had_error` is true. Setting
the flag mid-loop therefore kills the final EOF push (and any later token pushes). The
implementation must either stop at the first error and skip the EOF push, or separate the
allocation-failure gate from the general error flag. `error_msg` is a `const char *` with no
buffer field — a local `char buf[]` would dangle.

**Refinement settled 2026-11-26:** on error, `fprintf(stderr, "unexpected character '%c' at
position %zu\n", c, pos)` at the detection site, and return the list **truncated at the bad
byte with NO EOF token** (learner: "we didn't finish reading the entire message"). Print from
a literal format removes the need for any `error_msg` buffer; `error_msg` may stay a `const
char *` to a static label, with `error_pos` + `source[error_pos]` carrying the byte. EOF is
therefore a success-only token — the parser must check `had_error` before relying on it.
Stopping immediately also dissolves the sticky-`had_error` push gate: no push follows the
error. Tests must assert counts (e.g. `SELECT@` → 1 token, `@SELECT` → 0, unterminated string
→ 0).

**Rename DONE (2026-11-27):** `Token` fields are now `token_string`, `token_length`,
`source_index`. `Lexer.input_string` and the `tokenize` parameter keep `input_string`: they name
the whole source. Mechanical commit across header, scanners, tests and scratch harness; build is
clean under the strict flags. The suite still reports **41 tests, 4 failed** (the four valid
string tests) — the rename changed no behavior.

**Step 4 status after the rename (2026-11-27):** commit `331fd3c` contains the partial
`tokenize_string` loop described in `current.md`; it has NOT yet been made green. The success
branch still does not assign `token_length` / `source_index`, and the unconditional
`cursor_position += 1` after the loop runs even on the unterminated path — with the cursor
already at the NUL, that pushes it past the string and ASan reports a global-buffer-overflow in
`peek` from `unterminated_string_reports_opening_quote`.

**First edit checked on disk (2026-11-27):** learner moved the increment into the success branch
and added the field assignments. Partly right: the increment is conditional and the
`token_string` pointer targets content (`input + opening + 1`). Three defects remain: (1)
`source_index` was assigned a `const char *` instead of `starting_offset_position + 1` — the
build fails with `-Wint-conversion`; (1b) next attempt assigned the **byte**
`lexer->input_string[starting_offset_position]` (`'` = 39) instead of the index — compiler now
reports `-Werror=sign-conversion`; (1c) **fixed 2026-11-27:** `source_index =
starting_offset_position + 1`, build is clean; (2) `token_length` is computed *after* the increment and
subtracts `starting_offset_position`, so for `'hello'` it yields 7 instead of 5; (3) the stale
comment still describes an unconditional increment. The dispatch arm also still pushes the
string token unconditionally on error, letting `tkn_list_add` overwrite `error_pos`/`error_msg`.

**Remaining failure mechanism (2026-11-27):** on `'abc`, `error_reporting` correctly sets
`had_error = true`, `error_pos = 0`, `error_msg = "Unexpected character"` and prints the byte.
The string dispatch arm then calls `tkn_list_add` unconditionally. That function's else branch is
the *allocation-failure* path: it overwrites `error_pos` with `newToken.source_index` (1) and
`error_msg` with the realloc label. No new flag is needed — reuse the step-3 EOF guard shape:
push the string token only when `!had_error`. Then the lexical error survives, the list stays
empty, and the main loop exits via `had_error`.

**Length fix written by assistant on request (2026-11-27):** in the success branch,
`token_length = cursor_position - string_token.source_index` is computed **before** the closing
quote is consumed; the increment moved after it, and the stale comment moved with it. Verified:
build clean, **41 tests, 1 failed** — the four valid-string tests are green. Scratch shows
`'hello'` → `STRING pos=1 len=5`, `'hi'x` → `STRING pos=1 len=2` + `IDENT x`, `''` →
`STRING pos=1 len=0`. The remaining failure is the unterminated case: `error_pos` is 1 because the
dispatch arm still pushes on error and `tkn_list_add` overwrites it.

**Verified after the `source_index` fix (2026-11-27):** build clean; **41 tests, 5 failed**.
The four valid-string failures are all length +2 (`'hello'` expected 5, got 7; `''` expected 0,
got 2): the cursor is incremented before `token_length` is computed, and the subtraction uses
`starting_offset_position` (0) instead of `source_index` (1). The unterminated test now fails at
`error_pos` (expected 0, got 1) because the unguarded push reaches `tkn_list_add`'s else branch,
which overwrites the position with `newToken.source_index`. ASan reports leaks only because
failing assertions abort before those tests free their lists.

**Learner's decision (2026-11-27):** increment after the loop only if `closing_quote` is true.
On the NUL exit, leave the cursor alone — `had_error` terminates the main loop, so the extra
increment buys nothing and reads past the source. This is a branch of the flag the loop already
maintains, not a new state variable. Still to apply on disk; then the four valid-string tests
remain the checkpoint.

**Step 3 DONE (learner-written, 2026-11-26):** EOF push is guarded by `if (!had_error)` and
the `errorrrr` / `return NULL` tail is gone. His first version left `bool add_to_list` declared
but unused inside the guard — `-Werror=unused-variable` blocked the build; removed the variable
and kept `tkn_list_add(...)` as a bare call. Added the three Part 7 tests plus the count
assertion; **41 tests, 4 failed**, only the four valid-string tests still red. OOM path and lone
`!` remain open.

**Step 2 review (2026-11-26):** learner implemented `static void error_reporting(Lexer *,
size_t, const char *)` at `tokenizer.c:179` and calls it in the lone-`!` else and `default:`
branches, passing `starting_offset_position` before the cursor increment. Call sites and
position source are correct. Two gaps: (1) the printed line is `fprintf(stderr, "error_msg:
%s", error_msg)` — it omits both the byte and its position, so the console does not honor the
setted "print at detection site" decision (`unexpected character '%c' at position %zu`);
(2) the labels passed ("2nd char in; 2 char punctuation", "punctuation tkn invalid") are
internal descriptions, not user-facing messages. To name the byte, index the saved offset —
`lexer->input_string[starting_offset_position]` — not a cursor that may later be advanced;
`char` promotes to `int` for `%c`, so cast to `(unsigned char)` for bytes >= 128. Scratch
`"SELECT@"` confirms step 3 is still
needed: the EOF push reaches `tkn_list_add`'s else branch, which overwrites `error_pos` /
`error_msg`, then the `errorrrr` tail clears `had_error` and returns NULL (leak).

**Where `error_pos` comes from:** the main loop captures `starting_offset_position =
lexer.cursor_position` before dispatch (`tokenizer.c:218`). In the `default:` and lone-`!`
branches that value *is* the offending byte's index, provided the helper is called **before**
any `cursor_position += 1` for that byte. The unterminated-string case also passes the
branch's `starting_offset_position` (the opening quote), not the cursor after scanning. So
the reporting helper takes a `size_t position` argument and the caller supplies the captured
offset; the helper does not consume characters.
