# 60 — Open items / next steps

Part of the [notes tree](../ainotes.md). Read this to decide what to do next.

## Do next (Chapter 2 — tokenizer, in order)

1. **Fix the prefix-match bug** in `tokenize_word` (length check; see
   [41-tokenizer-state](41-tokenizer-state.md) defect 1). **The fix already exists in
   `stash@{0}`** — apply it, or retype it:
   `git stash apply stash@{0}`. Add tests first (new-test-first, guide ~line 2610):
   `"selection"` → one `TOKEN_IDENT`; `"SEL"`, `"SE"`, `"S"` → `TOKEN_IDENT`.
2. **Add the keyword table.** Only `"select"` is recognised; the other **10** keywords
   (`FROM, WHERE, INSERT, INTO, VALUES, CREATE, TABLE, DELETE, UPDATE, SET`) lex as
   `TOKEN_IDENT`. Put the table `static const` in `tokenizer.c` (not the header); match with
   `strncasecmp` **plus** the length check from step 1. Test case-insensitivity
   (`select`, `SeLeCt`).
3. **Stop the infinite loop on unhandled characters.** Any non-alpha, non-space char
   (`*`, `@`, a digit) spins forever because nothing advances the cursor. The `default`
   dispatch must consume the char (or record an error and exit). This unblocks the rest of
   the chapter.
4. **Add the remaining token types:** operators/punctuation (`*` `,` `;` `(` `)` `=` `!=`
   `<` `<=` `>` `>=`), numbers, and strings (quotes excluded; unterminated string →
   `had_error`). Absent helpers the guide lists: `peek_next`, `match`, `scan_number`,
   `scan_identifier`, `scan_string`.
5. **Implement `token_type_name`** from `tests/scratch.c` (see
   [42-token-type-name](42-token-type-name.md)) — recoverable, no rewrite.

Other checklist tests from the guide (~line 2610): position checks (`"SELECT * FROM"` →
`FROM` at position 9); `"SELECT@"` → error naming the character; identifiers with
digits/underscores (`user_id2`) but not starting with a digit; `token_list_free(NULL)` safe;
10,000-token stress input.

## Open design decisions (decide before the parser grows)

- [ ] **OOM reporting:** should `tokenize` return `NULL`, or the list with `had_error = true`?
      If it returns `NULL`, the caller has no list to read `had_error` from.
- [ ] **`had_error` is overloaded:** it is both the buffer-growth failure flag and the
      struct's general error flag (paired with `error_msg`/`error_pos`). A caller cannot tell
      "malformed SQL" from "out of memory". It is also sticky — never reset.
- [ ] **`token_type_name` spelling:** enum names (`"STAR"`, `"LTE"`) vs user-facing SQL symbols
      (`"*"`, `"<="`). The guide (~line 2863) derives `expect()` messages from it, so decide
      now.
- [ ] **EOF token fields:** `input_string` = start of input but `start_index` = end. Natural
      value is `lexer.input_string + lexer.cursor_position`. (`word_length = 0` is correct.)

## Pending cleanup (non-blocking)

- [ ] Rename `__EXIT` / `__CONTINUE` / `__CANTPARSE` (leading `__` is reserved for the
      implementation).
- [ ] `list_push`: `return false` on OOM rather than `assert`; single growth path.
- [ ] Remove the leftover `aaaaa` comment in `tokenizer.c`'s `tokenize` doc.
- [ ] Delete the `rescue-a1ce250` branch once you are confident nothing else is needed.
- [ ] Resolve the **two stashes** — apply `stash@{0}` (the prefix fix), drop `stash@{1}`
      (superseded). See [30-git](30-git.md).

## Repo / environment items (found 2026-10-16)

- [ ] **CI has never run.** The workflow is at `.github/workslows/ci.yml` — the directory is
      misspelled (`workslows`); GitHub only reads `.github/workflows/`. Rename the directory.
- [ ] **`.vscode/` is gitignored**, so the working debug config does not travel with the repo.
      It is documented in [20-debugging-vscode](20-debugging-vscode.md) for recreation.
- [ ] *(optional)* Enable `lazyvim.plugins.extras.lang.clangd` for a C LSP in Neovim (clangd is
      already installed). Offered, not yet actioned — see
      [21-debugging-neovim](21-debugging-neovim.md).
- [x] Reorganize the notes into this tree (2026-10-16).
