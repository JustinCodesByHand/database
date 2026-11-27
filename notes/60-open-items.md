# 60 — Open items / next steps

Part of the [notes tree](../ainotes.md). Read this to decide what to do next.

## Do next (Chapter 2 — tokenizer, in order)

- [x] **Prefix-match bug** — fixed and committed (length check in `tokenize_word`).
- [x] **`*` → `TOKEN_STAR`** — done; `star_input_is_star_then_eof` passes.
- [x] **Hang on unrecognised characters** — gone (they are consumed and skipped; see
  [41-tokenizer-state](41-tokenizer-state.md) defect 7).
- [ ] **Keyword table.** Only `"select"` is a keyword; add the other **10**
  (`FROM, WHERE, INSERT, INTO, VALUES, CREATE, TABLE, DELETE, UPDATE, SET`) as a `static const`
  table in `tokenizer.c` (not the header), matched with `strncasecmp` **plus** the length
  check. Test case-insensitivity (`select`, `SeLeCt`).
- [ ] **Make punctuation/operators real tokens:** `,` `;` `(` `)` first, then `=` `!=` `<`
  `<=` `>` `>=`. Today they are *silently dropped* (defect 7) — which also means
  `SELECT * FROM users;` parses `;` as nothing. Generalise the hand-rolled `*` branch
  (defect 8) into a small `switch`/table on `peek`.
- [ ] **Numbers and strings** (quotes excluded; unterminated string → `had_error`).
- [ ] **Implement `token_type_name`** from `tests/scratch.c` (see
  [42-token-type-name](42-token-type-name.md)) — recoverable, no rewrite.

Other checklist tests from the guide (~line 2610): position checks (`"SELECT * FROM users"`)
— note `FROM` should become `TOKEN_FROM` once the keyword table lands; prefix cases
`"selection"`/`"SEL"`/`"SE"`/`"S"` → `TOKEN_IDENT`; `"SELECT@"` → error naming the character;
identifiers with digits/underscores (`user_id2`) but not starting with a digit;
`token_list_free(NULL)` safe; 10,000-token stress input.

## Open design decisions (decide before the parser grows)

- [x] **OOM/error reporting — DECIDED 2026-11-26:** learner chose the partial list with
      `had_error = true`, `error_pos` at the offending byte, `error_msg` set; caller frees.
      Reason: "so I know where the error exists." Applies to lexical errors AND OOM; the
      `errorrrr` / `return NULL` tail in `tokenize` is now obsolete. Refined same day: error
      prints one line to stderr at the detection site and returns the list truncated at the
      bad byte with no EOF token; EOF is success-only.
- [ ] **`had_error` is overloaded:** it is both the buffer-growth failure flag and the
      struct's general error flag (paired with `error_msg`/`error_pos`). A caller cannot tell
      "malformed SQL" from "out of memory". It is also sticky — never reset.
- [ ] **`token_type_name` spelling:** enum names (`"STAR"`, `"LTE"`) vs user-facing SQL symbols
      (`"*"`, `"<="`). The guide (~line 2863) derives `expect()` messages from it, so decide
      now.
- [ ] **EOF token fields:** `token_string` = start of input but `source_index` = end. Natural
      value is `lexer.input_string + lexer.cursor_position`. (`token_length = 0` is correct.)
      Names updated 2026-11-27; defect still open.

## Pending cleanup (non-blocking)

- [ ] Rename `__EXIT` / `__CONTINUE` / `__CANTPARSE` (leading `__` is reserved for the
      implementation).
- [ ] `list_push`: `return false` on OOM rather than `assert`; single growth path.
- [ ] Remove the leftover `aaaaa` comment in `tokenizer.c`'s `tokenize` doc.
- [ ] Delete the `rescue-a1ce250` branch once you are confident nothing else is needed.
- [ ] Drop the remaining stash (`stash@{0}`, the old superseded `tokenizer.h` comment
      deletion). The prefix-fix stash was applied and committed. See [30-git](30-git.md).
- [ ] **Reconcile the diverged branch.** A 2026-10-16 rebase rewrote the already-pushed
      `a9074d0` into `8b266f9`, so `dev` is now **ahead 1 / behind 1** of `origin/dev` and the
      auto-push hook cannot fast-forward. Decide: `git push --force-with-lease` (keeps the
      squash) or reset to `origin/dev` and re-apply. See [30-git](30-git.md).

## Repo / environment items (found 2026-10-16)

- [ ] **CI has never run.** The workflow is at `.github/workslows/ci.yml` — the directory is
      misspelled (`workslows`); GitHub only reads `.github/workflows/`. Rename the directory.
- [ ] **`.vscode/` is gitignored**, so the working debug config does not travel with the repo.
      It is documented in [20-debugging-vscode](20-debugging-vscode.md) for recreation.
- [ ] *(optional)* Enable `lazyvim.plugins.extras.lang.clangd` for a C LSP in Neovim (clangd is
      already installed). Offered, not yet actioned — see
      [21-debugging-neovim](21-debugging-neovim.md).
- [x] Reorganize the notes into this tree (2026-10-16).

**Misconception 2026-11-27:** asked what the EOF token's `token_string` should be; learner answered
"the span of the start quote to the end quote" — that describes TOKEN_STRING, not EOF. EOF has
`token_length = 0`: its span begins and ends at one byte, the NUL at `cursor_position`. So
`token_string = lexer.input_string + lexer.cursor_position`, preserving the invariant
`token_string == input_string + source_index`.
