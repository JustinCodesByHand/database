# 41 — Tokenizer state & defects (`src/tokenizer.c`)

Part of the [notes tree](../ainotes.md). Read before working on the tokenizer.

## Status (verified on disk 2026-10-16)

**Build is green. 7 tests, 0 failed.**

```
repl
  ok   prints_prompt_before_reading
  ok   exits_on_exit_command
  ok   exits_cleanly_at_end_of_input
tokenizer
  ok   empty_input_is_single_eof
  ok   whitespace_only_input_is_single_eof
  ok   select_input_is_select_then_eof
  ok   star_input_is_star_then_eof
```

> **Corrections to earlier notes.** The alpha-loop hang is fixed; the **prefix-match bug is
> fixed and committed** (`word_length == strlen("SELECT") && strncasecmp(...)`); and the
> infinite loop on *unrecognised* characters is gone too (they are now consumed — see below).

## What the lexer handles now

Whitespace (skipped), alphabetic words, `*`, and EOF. Verified with the scratch harness:

- `SELECT * FROM users;` → `SELECT, STAR, IDENT(FROM), IDENT(users), EOF` — no hang.
- `*` → `STAR, EOF`.
- **Unknown characters** (`,` `;` `(` digits `@` …) are **consumed and silently skipped**.
  This is a deliberate stopgap so the main loop can never spin forever; they do **not** yet
  produce tokens or errors.
- `FROM` (and the other 10 keywords) still lex as `IDENT` — there is no keyword table yet.

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
| `tokenize_word(Lexer *, size_t)` | builds a word token; **now length-checked** |
| `tokenize(const char *)` | main loop: whitespace → word / `*` / skip-unknown |

## Open defects

1. **Only `"select"` is a keyword.** No keyword table; the other 10 keywords
   (`FROM, WHERE, INSERT, INTO, VALUES, CREATE, TABLE, DELETE, UPDATE, SET`) lex as
   `TOKEN_IDENT`. Needs a `static const` table in `tokenizer.c`, matched with `strncasecmp`
   **plus** a length check.
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
