# 90 — Session log (append-only)

Part of the [notes tree](../ainotes.md).

> **HISTORICAL.** This is the *why* behind the decisions. The code state described in older
> entries is often out of date and sometimes wrong (later entries correct earlier ones). For
> current facts read the topic files — this file is only for narrative and reasoning. **Do not
> delete old entries**; append new sessions at the bottom.

Kept because the *questions* are the reusable part; the answers are only correct for the code
state at the time.

## Session 2026-09-28 — "verify on disk, then what's next"

**Q: Is the brief's git/environment picture right?**
No. The brief said branch `parser`, `main` at `5ae9ff9`, a local `ch01-repl`. Disk showed branch
`main`, `main` at `4b84f70`, no `parser` locally, no `ch01-repl` at all, and `5ae9ff9` was the
skeleton commit. Also the host changed from Windows/WSL to native CachyOS Linux, so the
`wsl ... /mnt/c/...` build command is invalid here.

**Q: Which of the seven "known bugs" are actually still present?**
Five of seven were already fixed on disk (header static decl; `list_push` off-by-one; backwards
assert; `peek` debug `fprintf`; `token_list_free` NULL-safety). Still present: the empty loop
body (infinite hang), and the missing `advance` helper. Plus three blockers the brief never
mentioned — `is_whitespace` undefined, missing `}`, and `exitStatus` unused — which are why
`make test` never even ran.

**Q: Teach `switch`, not the finished code.**
Covered in [50-concepts-c](50-concepts-c.md): evaluate-once / match-and-jump / run-until-break,
what may be a case label, no case ranges in C, `-Wswitch` only for enums.

**Q (learning check 1): Why does a whitespace predicate take a `Lexer *`?**
Learner: because it doesn't need to advance; `advance` does that.
Correction: the pointer is there to **read** the cursor, not to move it. The real point is
`const` — a look-only predicate should take `const Lexer *` as a promise. Conclusion:
`is_whitespace` shouldn't exist; inline `isspace`.

**Q (learning check 2): Can `case '\0':` ever run?**
Learner: no, because `peek` is checked at the top of each turn.
Correct. Sharpen: `is_at_end` is true exactly when `peek == '\0'`, so the loop guard makes
`case '\0'` dead code. This is also why `advance` can be guardless, and why EOF is pushed
*after* the loop.

**Q (learning check 3): Does `advance` need a parameter, or does it already have `lex`?**
Learner: it should be `void` and increment; unsure about access to `lex`.
Correction: `lex` is a local inside `tokenize`; a file-scope function **cannot** see it. C has no
closures/implicit `this` — passing `&lex` is the only mechanism. And the guide wants `advance`
to **return `char`** (`start = advance(lex)` captures a token's first char).

**Q (learning check 4): What is OOM?**
Explained with a runnable demo (see [50-concepts-c](50-concepts-c.md)): `realloc` returns `NULL`
+ `ENOMEM`; the original block survives failure; `assert` aborts (exit 134). Re-posed, still
open: where should the OOM failure surface — `tokenize` returns `NULL`, or `had_error`?

**Q (learning check 5): Is `{TOKEN_EOF, lex.source, 0, lex.cursor_position}` a placeholder?**
Learner: the `0` is a placeholder until `list_push` sets real values; maybe use `-1`.
Correction: `list_push` sets nothing — it copies verbatim, so there is no later step. The `0` is
`word_length`, and `0` is **correct** for EOF. The questionable field is `start = lex.source`
(start of input) vs `position` (end of input) — inconsistent. `-1` is impossible anyway:
`word_length` is unsigned, so `-1` becomes `SIZE_MAX`.

## Session 2026-09-29 — scope, control flow, whitespace

**Q: With no parameters, what data can `advance` see? Does it see the caller's stack?**
No. A function sees only (1) its own parameters, (2) its own locals, (3) file-scope/`static`
variables. The caller's locals are **undeclared** in the callee — the compiler says
`error: 'lex' undeclared`, not "private". The stack frames are physically adjacent (demo showed
`&lex` and `&my_own_local` 32 bytes apart) but **adjacency is not access**; the language gives no
name to reach another frame. The only two bridges are parameters and globals. Hence `advance`
must take `Lexer *foo` (non-`const`, because it writes).

**Q: Do I need to return a space token?**
No. Whitespace is **skipped**, not tokenized: (a) the enum has no whitespace `tokentype` — the
type list is the contract; (b) the `whitespace_only_input_is_single_eof` test asserts exactly 1
token, so emitting them fails it. Whitespace still matters as a **boundary** (it terminates a
word) — you just never emit the terminator. Exception: inside a string literal, whitespace is
data and `scan_string` must keep it.

**Q: So `break` and `continue` affect the `while`, not the switch?**
Backwards. `break` targets the innermost `switch` **or** loop — in a case, it leaves the
**switch**, not the `while`. `continue` belongs only to **loops** (`continue` with no loop =
compile error) and skips to the enclosing loop's next iteration. In `tokenize` the switch *is*
the whole loop body, so both are equivalent there.

**Q: After `advance`, do I call `continue` to move to the next character?**
Almost — but **`advance` is what moves, `continue` moves nothing.** `advance` increments
`cursor_position`; `continue` only jumps back to the loop condition. `continue` without
`advance` = infinite loop on the same char. You also need `break`/`continue` to prevent
fall-through into the next case's body. Full write-up in [50-concepts-c](50-concepts-c.md).

## Session 2026-10-03 — OOM, realloc ownership, refactor review

**Q: What is OOM?**
Re-explained (was covered 09-29). OOM = "out of memory": `malloc`/`realloc` **return `NULL`**
and set `errno = ENOMEM`. They do not crash. Verified again that `assert` is **live** in this
build (no `-DNDEBUG` in the Makefile), so `assert(ptr != NULL)` turns an OOM into a SIGABRT
(exit 134). **Reconfirmed lesson:** on `realloc` failure the original block is still valid and
untouched.

**Q: Do I need to reassign the new address, or does `realloc` do it automatically?**
**No — `realloc` never writes to your variable.** Demonstrated:

```
before:  buf = 0x556b4e727010
after:   buf = 0x556b4e727010   <- UNCHANGED
         tmp = 0x556b4e728040   <- what realloc returned
```

Why: `realloc(ptr, n)` receives a **copy** of the address (pass-by-value), so it has no way to
write back — same rule as `advance` not being able to see `lex`. To modify a caller's variable
you must pass the *address of* the variable; `realloc` has no such parameter, so it returns the
value instead. One channel in, one channel out. The one-liner `p = realloc(p, n)` **leaks** on
failure (proven with ASan: `Direct leak of 16 byte(s)`). Also: on success `realloc` **frees the
old block**, so even *reading* the old pointer afterwards trips `-Wuse-after-free`. And
`realloc` may return the same address or a different one — never assume.

**Q: Can the grow function return an `int` with different codes?**
Yes, and it is one of the three legitimate C shapes. But an `int` is only one channel — growing
must convey *three* facts (new buffer, new capacity, success/failure). `int` earns its keep at
**3+ distinct outcomes**; with two outcomes `bool` is safer because `true`/`false` cannot be
misread (no `return 1;` meaning-success trap). **Consistency argument from this file:**
`list_push` and `add_tkn_to_tknlist` both take `TokenList *` and mutate it, so grow fits that
pattern and then only needs to return a single `bool` — the three-facts problem collapses with
no out-params.

**Q: Can I `switch` on capacity — `case 0` / `case full` / `default`?**
**No.** Compiler: `error: case label does not reduce to an integer constant`. Two deeper
reasons: (1) "full" is a *comparison between two runtime values* (`total == capacity`), and a
switch can only match a single value against constants — the information isn't in the
expression at all; (2) capacity has a huge value space (1,2,4,…16384), so most cases would never
match. **Rule of thumb: switch on a value from a small fixed set (a `char`, an enum); `if` on a
condition involving comparison or range.** `case 0` and `case full` also overlap at (0,0). Note
the `switch (peek(&lexer))` character dispatch is the *correct* use of switch.

**Q: How does my refactor look?**
Reviewed without prescribing fixes. Praised: `list_push` went 3 branches + dead fallthrough → 2
conditions; the doubling rule now lives in one function as the guide demands; clean how/when
separation; `token_list_free` is now silent. Flagged four defects (the infinite loop at 138, the
too-late NULL check at 47, the leak at 148, and the irrelevant variable in the growth condition
at 37-38).

**Q (clarified after review): what exactly is wrong with #2, #3, #4?**
Explained in full below — these are the three that cost the most time so far.

### New lessons from this session

- **Detecting failure ≠ handling it.** `list_push:75` checked `token_list == NULL`, but by then
  `grow_list_capisity:47` had already overwritten the only pointer to the live buffer. The check
  *detects*; it cannot *recover*. A check is only useful if it runs **before** the state it
  guards is destroyed.
- **One `malloc`, one `free`.** `TokenList` is two separate allocations (the struct, and the
  `token_list_buffer` inside it). Freeing the outer one does not free the inner one. This is
  exactly what `token_list_free` exists for.
- **`|` is not `||`.** `|` evaluates both sides and does a bitwise OR; `||` short-circuits. In a
  `while`, `isalpha(c) | c` is nonzero for almost any character, so the loop condition is almost
  always true.
- **A `void` function cannot report failure.** `grow_list_capisity` returning `void` means
  `list_push` has no way to learn whether the grow worked.

### Action taken this session

- **First green test run: 5 tests, 0 failed.** (`empty_input_is_single_eof`,
  `whitespace_only_input_is_single_eof`, plus 3 repl tests.)
- Renamed test-side field access `.type` → `.token_type` to match the header/tokenizer.c.
- Learner fixed defects #2 (OOM check), #3 (`free` → `token_list_free`) and #4 (irrelevant
  variable in the growth condition) unaided; all three verified by inspection. Also made
  `increment_cursor` `static`.
- Repaired two rename-round breakages: test field `.token_list` → `.token_list_buffer` (2
  lines), and the mangled `token_type_name` comment in the header (stray `}` + orphaned `*/`).
  Also removed the dangling `move_cursor` declaration.
- Refreshed the state notes to reflect the green build, the confirmed fixes, and the four new
  open items; added the standing instruction to keep the notes current.

### New lessons (2026-10-03, later)

- **A `static` function must never appear in a public header.** And you cannot fix a name
  mismatch by renaming the declaration to match — a non-static declaration followed by a static
  definition is itself a compile error. The only correct resolution when the implementation is
  file-private is to delete the declaration.
- **A function that can fail but returns `void` cannot report the failure.** The code gets the
  information out via a side effect (mutating `had_error`), which works but leaves no room to
  distinguish failure *kinds* — and makes the flag sticky.
- **A mangled `//` comment-out is a silent landmine.** Commenting out a line that also contains
  a brace swallows the brace and orphans the `*/`. The file still compiled, so only reading the
  header caught it.

### Docs worth keeping

- Crafting Interpreters, Ch. 16 "Scanning on Demand" —
  <https://craftinginterpreters.com/scanning-on-demand.html>
- `<ctype.h>` char classification (mind the `unsigned char` cast gotcha) —
  <https://en.cppreference.com/w/c/string/byte#Character_classification>
- `strncasecmp` (for case-insensitive keyword matching) —
  <https://man7.org/linux/man-pages/man3/strcasecmp.3.html>

## Session 2026-10-04 — first real word token; three string bugs

**Where it started:** recovered the orphaned 3PM commit, installed auto-push, added the
`"SELECT"` checklist test.

**The `SELECT` test did real work.** It exposed, in order:

1. **The alpha loop never advanced the cursor** (empty body) and used `|` instead of `||`.
   `isalpha(';') | ';'` = `0 | 59` = 59 → nonzero → true, so the condition was true for nearly
   every char. Both halves are now gone: the condition is a single `isalpha` call, so there is
   no bitwise/logical confusion left to get wrong. **Lesson: a loop that never changes what it
   examines can never terminate.** Look → decide → **move**.
2. **`#include <cstddef>` is C++**, not C (`<stddef.h>`). A C++ header in a `.c` file is the
   classic signal that C++ syntax has crept in.
3. **A token must never be built for a non-word.** Whitespace-only input ran the alpha loop
   zero times, so `word_length` was 0 — but the code pushed that empty token anyway, giving 2
   tokens instead of 1. Fix: guard the push with `word_length != 0`.
4. **`strncmp` is wrong for spans.** A token points *into* the caller's buffer, so
   `strcmp(input + start, "SELECT")` keeps reading past the word — for `"SELECT * FROM"` it
   compares `' '` against `'\0'` and reports no match. Must be length-limited. And `strcasecmp`
   is equally wrong (still NUL-terminated); the right tool is `strncasecmp` (`<strings.h>`,
   POSIX not ISO C).
5. **`strncmp` returns nonzero when the strings DIFFER.** The `if (strncmp(...))` form means
   "if this is *not* the keyword, make it the keyword" — inverted. Wants `== 0`. This one had
   been masked: case-sensitivity made every comparison fail, and the inverted logic turned that
   failure into "matched". Two bugs cancelling. `SELECT` passed for the wrong reason; `"users"`
   would have come out as `TOKEN_SELECT`.
6. **PREFIX MATCH.** `n = word_length` compares only that many bytes of the keyword, so `SEL`,
   `SE`, `S` all match `select`. Needs `word_length == strlen(keyword)`.

**Debugging lesson:** the "memory leak" was never a leak. LeakSanitizer reported an abandoned
allocation because the test **aborted at a failed `assert` before reaching `token_list_free`.**
A leak report after a test failure is a *symptom*. Read the assertion first; fix it and the leak
disappears. `assert` is live here (no `-DNDEBUG`), and `abort()` discards block-buffered stdout
when piped — so `make test` can show *only* a leak report and no test output. Use
`script -qec ./build/tests /dev/null` to get a pty and see the results.

**Also:** the editor twice saved a stale buffer over edits (`#include <cstddef>` and a
`(void)starting_index;` both reappeared). Close/reopen the file before editing or you will keep
losing work — including tests.

**Environment gotcha:** `stdbuf` conflicts with ASan's `LD_PRELOAD` ("ASan runtime does not come
first in initial library list"). Use `script` for a pty instead.

## Session 2026-10-16 — VS Code debugging; the 27-case body is found

**Goal:** debug `make test` in VS Code. It did not work.

**Root cause:** three extensions had each written a launch config, and the active one had
`"program": ".../tests/build/Debug/outDebug"` — the C/C++ Runner extension's build convention,
pointing at a file that does not exist. The Runner never reads the Makefile; `settings.json` was
100% `C_Cpp_Runner.*`. Also `c_cpp_properties.json` was **0 bytes** (invalid JSON). Fixed by
writing a plain `cppdbg` + `tasks.json` setup aimed at `build/tests`, `build/minidb`,
`build/scratch`.

**Discovery that corrects two earlier claims.** `tests/scratch.c` contains the **complete
27-case `scratch_type_name`** — i.e. the `token_type_name` body twice reported as lost. The name
never matched a `git grep token_type_name` because it is spelled `scratch_type_name`; the only
occurrence of the literal string `token_type_name` in that file is a **comment**. Lesson:
grepping for a *name* cannot find a function that was renamed, and "no commit contains it" was a
conclusion drawn from that failed search. **Search for the shape (a `switch` over `tokentype`),
not the name.**

`scratch.c` was itself broken by the rename and is now fixed: `TokenType`→`tokentype`,
`TokenList.token_list`→`token_list_buffer`, `Token.position`→`start_index`,
`TOKEN_ERROR`→`token_error`. It is the best first debug target:

```sh
make build/scratch && ./build/scratch SELECT
```

**Makefile trap:** `make test` builds **and runs**; `make build/tests` builds **only**. A
`preLaunchTask` must use the file target, or the task runs the program and then the debugger
launches it a second time. And every `preLaunchTask` needs a `problemMatcher` (`$gcc`) or VS
Code can hang before ever reaching the debugger.

**`token_type_name` is therefore recoverable** — rename, move to `tokenizer.c`, change
`case TOKEN_ERROR:` to `case token_error:`, and reconcile the `default:`. No rewrite.

## Session 2026-10-16 (later) — LazyVim debugging

**Goal:** get the same debugging working in LazyVim, reusing the `build/tests` / `build/minidb`
/ `build/scratch` targets from the VS Code work.

**Root cause:** the LazyVim DAP extra was never enabled — `lazy.lua` imported only
`lazyvim.plugins` and `plugins`, so `nvim-dap` was not installed at all.

**Fixed, both outside the repo:**
- `~/.config/nvim/lua/config/lazy.lua`: added `{ import = "lazyvim.plugins.extras.dap.core" }`
  before `{ import = "plugins" }`.
- `~/.config/nvim/lua/plugins/dap.lua` (new): defines the codelldb adapter (guarded, so it also
  works if `lang.clangd` later defines it) and three `dap.configurations.c` entries that
  `make -C <root> build/<x>` then debug `build/<x>`.

**Reading LazyVim's source paid off.** `dap.core` deliberately defines *no* adapter and *no*
`dap.configurations.c` — its comment says "Requires language specific adapters ... (see lang
extras)". The adapter + generic C configs live in `lang/clangd.lua`. So the first useful act was
to read `~/.local/share/nvim/lazy/LazyVim/lua/lazyvim/plugins/extras/dap/core.lua` and
`.../lang/clangd.lua` instead of pasting a config from memory.

**Two merge facts, confirmed in lazy.nvim source rather than assumed:**
- `lazy/core/plugin.lua` `M._values` runs *all* `opts` functions for a plugin in spec order
  (super first), so a later `opts` in `plugins/` overrides an earlier extra's `opts`.
- `lazy/core/loader.lua` `M.config` resolves `opts` *then* runs `config`, so `mason-nvim-dap`'s
  handler (called from `dap.core`'s `config`) runs after every `opts` and appends four
  `LLDB: Launch*` configs *behind* the custom three.

**Traps re-confirmed:** (1) mason installs are on Neovim's PATH, not the shell's —
`command -v codelldb` lied; the file existed all along. (2) `make test` builds *and runs*; launch
configs must rebuild the *file* target (`build/tests`), not `test`. (3) codelldb bundles its own
LLDB, so the missing system `lldb` was never a blocker.

**Verified headlessly** with `nvim --headless` + `require("lazy").load`:
`dap.adapters.codelldb` resolves to the mason binary; all three `program()` calls build and
return executable paths; and a real `dap.run` fires `DAP_EVENT_INITIALIZED` then
`DAP_EVENT_EXITED`. That is the pattern for checking any "is nvim actually configured" question
without a GUI.

**Not done (offered):** `lang.clangd` is *not* enabled, so there is still no C LSP
(hover/definition/completion). clangd is already installed by mason. Enabling
`lazyvim.plugins.extras.lang.clangd` adds it, and its own `opts` defines the codelldb adapter —
the guard in `plugins/dap.lua` makes the two coexist.

## Session 2026-10-16 (evening) — dap-ui usage; make it repeatable

LazyVim debugging confirmed working. Three follow-ups:

1. **Reproducibility:** added `scripts/setup-nvim-dap.sh` (idempotent; backs up; inserts the
   extra in the right load-order position and writes `plugins/dap.lua`). Tested against a
   sandbox `XDG_CONFIG_HOME` before trusting it on the real config. Motivated by redoing this on
   a second machine.
2. **Keybindings + dap-ui usage** written up (now in
   [21-debugging-neovim](21-debugging-neovim.md)).
3. **Resizing** answered from source, not memory: dap-ui panels set
   `winfixwidth`/`winfixheight` (`layout.lua:179-180`), which is why `<C-w>>` does nothing.
   Workarounds: clear the winfix options for the session, or override `layouts` — remembering
   that `config.lua:194-196` makes a supplied `layouts` **replace** the default entirely.

Methods that paid off twice today: read the plugin's own source/config before answering
(`nvim-dap-ui`), and read `lazy.nvim`'s loader to confirm load order. Both times a
plausible-sounding answer from memory would have been wrong.

## Session 2026-10-16 (night) — notes reorganized into a tree

Split the single 1050-line `ainotes.md` into this tree: `ainotes.md` is now a **root index**
with a routing table ("read only the branch for your task"), and the detail lives in
`notes/*.md`. Rationale: the old file was read in full every session, most of it irrelevant.

Also, while verifying the split against disk, found and recorded:

- **CI has never run** — `.github/workslows/ci.yml` (misspelled directory).
- **`stash@{0}` holds the prefix-match fix** (the pending task!); `stash@{1}` is the superseded
  header-comment deletion. Recorded in [30-git](30-git.md).
- Corrected stale claims: the alpha-loop hang is fixed (a *different* hang on unhandled chars
  remains); `token_type_name` is absent, not "added"; the field rename is done, not WIP.

**Lesson:** the same "trust disk over the brief" rule that caught the 09-28 git picture also
applied to these notes. Reorganizing is a good moment to re-verify every claim.

**Immediate correction:** committing the reorg showed the post-commit hook **pushed**
(`dev -> origin/dev`), and `gh auth status` reports logged in as `JustinCodesByHand`. The notes'
"auto-push cannot push — auth never configured" claim was therefore stale. Updated
[30-git](30-git.md): `dev` is in sync with `origin/dev`, and the second-machine blocker is gone
(a plain `git pull` now gets `scripts/setup-nvim-dap.sh`).
