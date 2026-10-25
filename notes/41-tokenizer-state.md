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
   lexer.cursor_position}` sets `input_string` to the *start* of input but `start_index` to the
   *end*. `word_length = 0` is correct. Natural value: `lexer.input_string +
   lexer.cursor_position`.
7. **Unknown characters are silently dropped** (the `else` in `tokenize`). Fine as a hang-guard,
   but there is no error reported yet — the guide wants e.g. `"SELECT@"` to name the character.
   Also blocks `,` `;` `(` `)` and all comparison operators, which the chapter needs next.
8. **Single-character tokens are hand-rolled.** `*` is built inline
   (`{TOKEN_STAR, input + start, 1, start}`). That is fine for one operator; the other operators
   will want a small table or a `switch` on `peek`.
9. **Stale artifact:** an `aaaaa` fragment in the `tokenize` doc comment.

**Absent helpers the guide lists:** `peek_next`, `match`, `scan_number`, `scan_identifier`,
`scan_string`.
