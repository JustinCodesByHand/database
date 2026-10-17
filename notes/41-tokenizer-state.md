# 41 — Tokenizer state & defects (`src/tokenizer.c`)

Part of the [notes tree](../ainotes.md). Read before working on the tokenizer.

## Status (verified on disk 2026-10-16)

**Build is green. 6 tests, 0 failed.**

```
repl
  ok   prints_prompt_before_reading
  ok   exits_on_exit_command
  ok   exits_cleanly_at_end_of_input
tokenizer
  ok   empty_input_is_single_eof
  ok   whitespace_only_input_is_single_eof
  ok   select_input_is_select_then_eof
```

> **Correction to the old §6:** item 1 there ("alpha loop infinite loop — STILL OPEN") is
> **stale**. The alpha loop now advances (`while (isalpha(...)) { increment_cursor(...); }`)
> and `SELECT` tokenizes correctly. However, a **related** infinite loop remains — see below.

## What the lexer actually handles now

Only three things: **whitespace** (skipped), **alphabetic words**, and **EOF**. Everything
else is unhandled:

- Operators, punctuation, numbers, and strings produce **no token**.
- Worse: any non-alpha, non-space character (e.g. `*`, `@`, a digit) makes the outer loop
  **spin forever** — the alpha loop does nothing, `word_length` is 0, the zero-length guard
  skips the push, and nothing advanced the cursor. So `SELECT * FROM t` hangs after `SELECT`.

That hang is the practical blocker for the rest of the chapter.

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
| `tokenize_word(Lexer *, size_t)` | builds a word token; **has the prefix bug** |
| `tokenize(const char *)` | the main loop |

## Open defects

1. **Prefix-match bug in `tokenize_word` (open item 9).** It does
   `strncasecmp(input, "select", word_length) == 0` with `n = word_length`, so only that many
   bytes of the keyword are compared. Verified standalone:
   `SELECT`→SELECT, `SEL`→**SELECT (wrong)**, `SE`→**SELECT (wrong)**, `S`→**SELECT (wrong)**,
   `selection`→IDENT, `users`→IDENT.
   **Fix: also require equal lengths.**
   `word_length == strlen("SELECT") && strncasecmp(input, "SELECT", word_length) == 0`.
   (`selection` is only correct *by accident*: its length exceeds the keyword's, so the
   comparison runs into the keyword's NUL. The length check makes both directions deliberate.)
   SQL impact: `SEL * FROM t` would silently parse as a `SELECT`.
   **A fix for exactly this is sitting in `stash@{0}` — see [30-git](30-git.md).**
2. **Only `"select"` is recognised.** There is no keyword table; every other keyword lexes as
   `TOKEN_IDENT`. Needs a table (`static const`, in `tokenizer.c`, not the header) matched with
   `strncasecmp` + a length check.
3. **`list_push`'s return value is ignored at the word push site** (`tokenize`)
   `list_push(..., word_tkn);` — the `bool` is discarded, so a grow failure is not seen there.
   (The EOF push *does* check it.)
4. **`grow_list_capisity` returns `void`.** Failure travels as a side effect (`had_error`)
   rather than a return value. The function that can fail cannot report it.
5. **`had_error` is sticky.** Once true, `list_push` returns `false` for every later push even
   when there is room. It is also the struct's *general* error flag (paired with
   `error_msg`/`error_pos`), so reusing it for OOM means a caller cannot distinguish
   "malformed SQL" from "out of memory". Still an open design decision.
6. **`token_type_name` is not implemented.** Declaration commented out in the header; nothing
   calls it. See [42-token-type-name](42-token-type-name.md).
7. **EOF token fields are inconsistent.** `{TOKEN_EOF, lexer.input_string, 0,
   lexer.cursor_position}` sets `input_string` to the *start* of input but `start_index` to the
   *end*. `word_length = 0` is correct. Natural value would be
   `lexer.input_string + lexer.cursor_position`.
8. **Stale artifact:** an `aaaaa` fragment in the `tokenize` doc comment.

**Absent helpers the guide lists:** `peek_next`, `match`, `scan_number`, `scan_identifier`,
`scan_string`.
