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

## Session 2026-10-16 (night-end) — `*` token, divergence, and notes cleanup

**Goal:** get `*` to tokenize as `TOKEN_STAR`.

Added the test `star_input_is_star_then_eof`; it hung (exit 124) demonstrating the infinite
loop on unrecognised chars. Implemented minimal handling in `tokenize`:
- guard `is_at_end()` after whitespace skip (so trailing whitespace doesn't fall through),
- handle alphabetic words as before, **handle `*` as a single-char token** (span 1 char,
  `start_index` and `word_length` correct), and **consume unrecognised chars** to prevent hangs
  (they're silently dropped for now with a TODO).

Results: build green, **7 tests, 0 failed**. Verified `SELECT * FROM users;` → `SELECT, STAR,
IDENT(FROM), IDENT(users), EOF` (no hang), `*` → `STAR, EOF`, `@`/`SELECT@` → no hang, no
leak. Commits: `8b266f9` (notes + the squashed prefix fix) is ahead 1 / behind 1 of
`origin/dev` (diverged) because a rebase rewrote an already-pushed commit; the auto-push hook
won't force. One stale stash remains (the header comment deletion). Updated
[41-tokenizer-state](41-tokenizer-state.md), [60-open-items](60-open-items.md),
[30-git](30-git.md) and the root `ainotes.md` to reflect current reality.

**Git note:** local `dev` was rewritten; `git push --force-with-lease` will be needed if you want
to sync `origin/dev` to `8b266f9`. The notes now document this.

## Session 2026-10-16 (later) — LazyVim DAP troubleshooting list + git/auth

The learner hit an error setting up DAP in LazyVim on a second laptop and asked for an explicit
step-by-step list (not another script) that the other session could follow to fix the config.
Added `notes/22-debug-lazyvim-troubleshoot.md` (commit `94efd02`, pushed to `origin/dev`).
It is a **checklist**, not a fix — it captures what to run and what output to send, because the
specific error was never provided. Later renamed to
[23-debug-lazyvim-troubleshoot](23-debug-lazyvim-troubleshoot.md) to avoid colliding with
[22-reproduce-debugging](22-reproduce-debugging.md), cross-linked both ways, and added it to the
routing table + layout in the root.

**Git/auth:** pushes had been failing with `could not read Username for 'https://github.com'`.
Root cause was **not** the token: gnome-keyring (`Default_keyring`) was **locked**, so gh could
not read its OAuth token and the `gh auth git-credential` helper returned nothing (this is also
why lazygit "forgot" the token). Options given: (A) switch `origin` to SSH (an unencrypted
`~/.ssh/id_ed25519` already exists) — durable, no keyring; (B) re-auth gh with
`--insecure-storage` so the token lives in `~/.config/gh/hosts.yml` instead of the keyring;
(C) unlock the keyring (recurs after reboot without PAM integration). `origin/dev` now carries
`94efd02`, so a push has since succeeded — treat the auth state as resolved but verify before
relying on it.

## Session 2026-10-16 (later still) — DAP "port error" investigation

Learner reported a **"port error" from DAP when starting the debugger**. Investigated on the
primary machine:

- Registered adapter is correct: `type = "server"`, `port = "${port}"`, command =
  `~/.local/share/nvim/mason/bin/codelldb` (written by `lua/plugins/dap.lua`).
- codelldb supports `--port`; `codelldb --port 55123` **listens** on `127.0.0.1:55123`.
  (`codelldb --version` is unsupported — errors, expected.)
- A headless launch of `build/tests` completed the handshake — `~/.local/state/nvim/dap.log`
  shows codelldb connecting and the debuggee starting via
  `codelldb-launch --connect=127.0.0.1:43933`. So **no port problem on the primary machine**.
- Found `~/.config/nvim/dap.lua` (nvim config ROOT, outside `lua/`) defining an OLD
  `type = "executable"` codelldb adapter that passes `${port}`. It is **not** loaded here
  (verify: `require` searchpath does not include config-root `dap.lua`), but it is the classic
  cause of the port error where it *is* loaded: nvim-dap only substitutes `${port}` for
  `type = "server"`, so `type = "executable"` sends the literal `${port}` to codelldb.

Root cause (on the second laptop) is therefore almost certainly a stray/legacy executable-type
adapter at `dap.lua`, `lua/dap.lua`, or (worst) `after/plugin/dap.lua`. Wrote a full diagnosis +
checks into [23-debug-lazyvim-troubleshoot](23-debug-lazyvim-troubleshoot.md) (new "port error"
section), and hardened `scripts/setup-nvim-dap.sh` with a step 3 that moves such legacy files
aside (backed up, not deleted). Updated [22-reproduce-debugging](22-reproduce-debugging.md) and
the root focus. `bash -n` clean; step-3 logic dry-run-tested. Pushed so the other laptop can
pick this up.

## Session 2026-10-17 — DAP "port error" root cause found and fixed

Learner asked to search the LazyVim debug config using the new note 23. Ran note 23's core check:

```sh
grep -rn "dap.adapters.codelldb" ~/.config/nvim --include='*.lua'
```

It found **two** files defining the adapter:
`lua/plugins/dap.lua` (the script's correct one, with a mason-path fallback) and
`lua/plugins/dap-c.lua` (an old hand-written leftover). Note 23's three known-bad paths
(`dap.lua`, `lua/dap.lua`, `after/plugin/dap.lua`) did not include this one.

Verified which spec won:

```sh
nvim --headless -c "lua require('lazy').load({plugins={'nvim-dap'}}); for _,c in ipairs(require('dap').configurations.c) do print(c.name) end" -c "qa!"
# -> Debug build/tests / Debug build/scratch  (dap-c.lua's stale configs, NOT "minidb: ...")
```

So this was **not** the `type = "executable"` port bug the notes expected. It was a
**duplicate-adapter** bug: `dap-c.lua` won the load race and replaced both the adapter and the
configurations. Its adapter used `command = vim.fn.exepath("codelldb")` with **no mason
fallback** — where that is `""`, codelldb cannot start, which is the port error.

Fix: moved `lua/plugins/dap-c.lua` aside (`*.legacy.<timestamp>`). Re-checked: configs are now
`minidb: build + debug tests/minidb/scratch`, adapter `type = "server"`, command
`~/.local/share/nvim/mason/bin/codelldb`.

Hardened `scripts/setup-nvim-dap.sh` step 3: instead of three hard-coded paths it now greps
every `*.lua` under `~/.config/nvim` for `dap.adapters.codelldb` and moves aside any file except
the one it wrote. `bash -n` clean; ran twice — second run reports `already up to date` and moves
nothing (idempotent). Updated notes 22, 23 and the root focus.

## Session 2026-10-23 — LearnHarness teaching system migrated to `AGENTS.md`

The learner hit a "free tier" error using their OpenCode Go key with LearnHarness
(github.com/amosblomqvist/learnharness, AGPL-3.0). Diagnosis for the record: opencode's
`-free` models are restricted to the opencode client itself ("Free-tier chat models require
OpenCode; use paid models or Go from other clients"). The learner says a paid model was
selected, so remaining suspects were: a stale `OPENCODE_API_KEY` shadowing the in-app key
(LearnHarness precedence: real env / `.env.local` always wins — Settings→Keys shows
`shadowedByEnv`), the key attached to the wrong provider row (Zen and Go share the same env
var), or a key generated before the subscription (regenerate). DROPPED at the learner's
request — focus moved to the migration.

Migration done: read LearnHarness's whole teaching system (system-prompt.md; prompts/shared,
teacher, prober, practice fragments; docs/teaching-methodology.md, probe-teach-split.md,
plan.md, vision.md; skills/learnharness/SKILL.md) and adapted it into **`AGENTS.md`** at the
repo root — the file OpenCode auto-loads into every session. It carries: the mission
(understanding over recitation, the click, system absorbs logistics); measure-before-teach
(bracketing strands, enter near the top); minimal-tree planning (stress-test roots, `todo` =
the running plan map); one reasoning step at a time (Principle i — unconditional truths;
Principle ii — "how could I have discovered this?"); the predict→teach→check cycle; check
principles (application over recitation, diagnostic misses); Socratic vs expository;
deliver-on-request-then-teach; the practice doctrine (inactive-state retrieval, interleaving,
hint ladder R0–R5, fresh-surface retests); accuracy (verify doubts before teaching, compile
don't assert, trust the repo over the notes); the voice; formatting; this project's working
rules; and the session protocol. Attribution and link to LearnHarness included; it is an
adaptation, not a copy. `ainotes.md` now records that `AGENTS.md` is always in context.

## Session 2026-10-23 (later) — C++ autocomplete fixed: clangd pinned to C

The learner reported C++ completions while writing C. Diagnosis (headless nvim): `.c` buffers
were fine (`ft=c`, clangd attached) but `.h` buffers were `ft=cpp` — and `nvim --clean` proved
`.h → cpp` is **Vim's own default** (`:h filetype` documents `g:c_syntax_for_h`), not
LazyVim's. clangd defaults ambiguous headers to C++ as well — same trap, LSP side. There is
no separate "C LSP"; clangd serves both languages, per file.

Fixes, all verified on disk/run:
1. repo `.clangd` — `Add: [-xc, -std=c17]` for `.*\.(c|h)$`; `clangd --check=src/tokenizer.h`
   parses as C with 0 errors.
2. `make compile_commands` (new Makefile target) — generates `compile_commands.json`
   (gitignored, machine-specific) from the Makefile's own variables. First attempt put
   `-Isrc -Itests` into `.clangd` — broken: clangd's *fallback* runs each file with its own
   directory as cwd, so relative `-I` silently misresolved and produced 21 phantom errors on
   tests/test_tokenizer.c (`'tokenizer.h' file not found`). The database fixes the `.c` files;
   the `.clangd` block covers headers (which never get database entries).
3. `vim.g.c_syntax_for_h = true` at the top of `~/.config/nvim/init.lua` — verified `.h`
   buffers now `ft=c`. (An earlier attempt in `lua/config/filetypes.lua` never loaded —
   removed.) Restart of the running nvim needed to pick it up.

Parity proof the setup works: clangd now reports the exact `-Wunused-const-variable` warning
gcc does, with the same flags. `clangd --check` (not `--check-path`) is the verification tool;
its `tweak: … FAIL` lines are internal noise. The stale todo item about enabling the clangd
extra is corrected — it was already enabled via `lazyvim.json`. DAP: no change; codelldb is
language-agnostic.

Meanwhile the learner is mid-worksheet on the keyword table (table written at
tokenizer.c:107, loop pending): the current `-Werror=unused-const-variable` build failure is
exactly that in-between state, not breakage — the stage-1 checkpoint (identical red) lands
when the loop replaces the if/else.

## Session 2026-10-25 — keyword table completed; merge incident + repair; main is home

The learner finished the keyword-table worksheet solo (11 entries, the loop, `TOKEN_IDENT`
default before the loop — the safe landmine placement). Review found the **span bug**: the
third `strncasecmp` argument was `strlen(newToken.input_string)` — the source's remaining
length, not the word's — so every keyword followed by more input regressed (`SELECT * FROM
users;` → SELECT itself became IDENT). The 11-test suite passed anyway **by coincidence**:
every test input ended right after the word. The learner fixed it (`word_length`) after the
sizeof/strlen lesson (pointer = 8 bytes; the string behind it only strlen knows — instrument
in the transcript). The **multi-word guard** `select_from_users_full_statement` (types +
positions — also the guide's "position correctness" item) is now in: **12 tests, 0 failed**;
the scratch instrument reads `SELECT, STAR, FROM, IDENT(users), EOF`. A debugger recipe for
watching the span live was written into notes/40 (`ff55fbd`).

**The merge incident.** The learner merged `dev` into `main` in lazygit; the conflict picks
took the stale Oct-3 side in five files (pre-rename `TokenType`/`lex_tkn_list`, the 491-line
pre-tree ainotes, a franken test file from interleaved picks), and the broken merge was
pushed. Repaired (`dd55245`) by taking dev's five files from the merge's own second parent.
Two lessons recorded in notes/30: every hunk pick IS the resolution (`b` interleaves); and
`make test` gates every merge before push.

**Editor clobbering struck again** mid-session (a stale nvim buffer saved the old test file
over the new one). That nvim session predates the eva01 theme, the clangd
`--limit-results=4` wiring, and the init.lua revert — it still needs a full `:qa!` and
reopen. `main` is the working branch now (learner's move); `dev` is retired; the hook pushes
main. Next: punctuation/operators as real tokens.
## Session 2026-10-26 — ordered tokenizer exercise guide

Learner asked for a `tokenizer.md` modeled on the direction/pacing of Ziglings and
50-small-c-exercises: ordered tasks, user-visible contracts and examples, and comments/hints
that leave the implementation to the learner while preserving enough context to stay
oriented.

Read both references plus the tokenizer chapter in `src/minidb-c-guide.md`. Created root
`tokenizer.md`: current verified baseline (12/0), shared source-span invariant, ten ordered
checkpoints from punctuation through full-statement verification, test-first work loop,
dependency-aware context, and a graduated hint bank at the end (nudge → stronger conceptual
hint, no completed implementation). It explicitly leaves design forks (lexical-error return
contract, string escapes, EOF convention, type-name display) undecided rather than guessing.
Added tokenizer.md to the `ainotes.md` routing/current focus and corrected the stale `todo`
branch/status label (`main`, current working branch).

## Session 2026-10-30 — current.md rewritten as the full tokenizer roadmap

Learner reported the current task finished; disk says otherwise in part: keyword table is
committed (12/0), but punctuation is NOT implemented (tree clean, silent-skip `else` still
present). Flagged prominently atop the new card — possible unsaved editor buffer; check
before writing over it. Rewrote tracked `current.md` as the ordered road to the end of
Chapter 2 (parts 1-10: punctuation → 1-char ops → 2-char ops → identifier spelling → numbers
→ strings → unknown-char errors + the NULL-vs-had_error contract decision → EOF/list
robustness/stress → token_type_name → the full-statement checkpoint), each part = Context /
Questions (the implementation is the learner's) / test-first checkpoint, with the span
invariant as the header and the per-part hint bank in tokenizer.md as the escape hatch.

## Session 2026-11-26 — current.md Parts 6-10 expanded; Parts 1-5 confirmed green

Learner asked to expand `current.md` Parts 6-10 with everything needed. Re-read the guide's
Chapter 2 skeleton/test list/Done-when (lines 2468-2665), `tokenizer.h`, `tokenizer.c`,
`tests/scratch.c`, and notes 40/41/60 before writing. Replaced the five short part cards with
detailed ones: a dispatch mermaid showing where each part plugs in; Part 6 (three positions,
`word_length = close - open - 2`, the `size_t` underflow trap, no escapes, `ASSERT_EQ_MEM`
not `ASSERT_EQ_STR` for spans, exact expected spans for five inputs); Part 7 (partial-list vs
`NULL` contract table, `error_msg` storage problem, sticky-`had_error` trap, exact error
positions); Part 8 (EOF pointer-at-end convention, realloc/growth call sites, four robustness
tests incl. 10,001-token stress); Part 9 (names choice, `-Wswitch` vs `default`, recovery from
`scratch_type_name`); Part 10 (all ten exact token spans for the full statement, the six-token
grammar-free case, the guide's Done-when).

Verified disk before writing the banner: the earlier red Part 5 state had been fixed and
committed by the learner (`1adf1fb`) — **33 tests, 0 failed**. Corrected the top banner and
Part 5 card to match (initially drafted them from the stale checkpoint, then re-ran
`build/tests` and fixed). Updated `todo` to mark Parts 1-5 done and restate Parts 6-10 as the
active queue. No tokenizer code changed in this session.

### Continued — Part 6 tests written (test-first)

Learner asked for the Part 6 tests. Added five to `tests/test_tokenizer.c`, registered in
`suite_tokenizer` under a `/* Part 6 - quoted strings */` block: quote-excluded span
(`'hello'` → STRING pos 1 len 5, EOF pos 7), interior whitespace (`'a b'` → len 3), empty
string (`''` → pos 1 len 0), no swallowing of trailing bytes (`'hi'x` → STRING + IDENT), and
unterminated `'abc` → `had_error` + `error_pos 0`. Used `ASSERT_EQ_MEM` for token text
(spans are not NUL-terminated) and a pointer-identity `ASSERT` for `input_string == input +
1`. Result: **38 tests, 5 failed** — all five red, prior 33 green. Scratch harness on
`'hello'` shows why: today the quotes are silently consumed by the `default:` branch and
`hello` is scanned as IDENT with the *correct* span, so the implementation mostly needs to
reclassify and gate on the closing quote. The unterminated test deliberately pins the
partial-list error contract; it is the one to rewrite if the learner picks return-NULL.

### Error contract decision (same session)

Asked which error contract he wanted. Answer: partial list, *"so I know where the error
exists."* Recorded as settled in `current.md` Part 7, `notes/41`, `notes/60`, and `todo`;
updated the test comment to match (it no longer hedges about return-NULL). Consequence taught:
the partial-list choice makes `error_pos` the primary artifact, but `tkn_list_add` gates on
the sticky `had_error`, so recording an error mid-loop will refuse the EOF push unless the
scanner stops at the first error or the gate is separated from the general flag. One contract
now covers OOM too — the existing `errorrrr`/`return NULL` tail is obsolete.

**Refinement proposed by the learner (not yet settled):** print the error message to the
console and return the list *truncated at the error*. Observed consequence: recording the
error, stopping immediately, and skipping the EOF push means `tkn_list_add`'s sticky gate
never fires — the trap dissolves. Printing at the detection site via a literal `fprintf`
format also removes the need for a fixed `error_msg[20]` buffer. Open sub-questions: stderr
vs stdout, whether the truncated list still carries EOF, and one print site vs caller-owned
printing.

**Settled same session:** no EOF on the error path — learner: *"we didn't finish reading the
entire message."* So the list stops at the last valid token, and stderr gets one line at the
detection site. Writing that line from a literal format kills the `error_msg` buffer question.
`current.md` updated: invariant, dispatch mermaid, Part 7 contract/test table, Part 8 EOF
section and test table. Tests must pin counts (`SELECT@` → 1, `@SELECT` → 0, unterminated → 0)
because "truncated" is otherwise unasserted.

### Formatting / lint pass (same session, on request)

Learner asked to fix all indentation, formatting and linting. Added `.clang-format` (LLVM,
2 spaces, 80 cols) and the missing `make fmt` target (Makefile already listed `fmt` in
`.PHONY` but had no rule). Ran focused `clang-tidy --fix` for else-after-return, implicit
bool conversion, trailing commas, redundant parens, braces, and declaration/definition
parameter names; then `make fmt` over all `.c`/`.h` in `src/` and `tests/`. Manually renamed
the reserved `__EXIT`/`__CONTINUE`/`__CANTPARSE` enum values to `REPL_*`, added missing direct
includes (`<stdint.h>` in hex.c, `<stdio.h>`/`<stdlib.h>` in test_main.c, `<stdio.h>` in
test_repl.c), matched header parameter names to definitions, and cleaned comment typos plus
the `aaaaa` leftover. clang-tidy's auto-fix inserted `(int)` casts on `had_error`; replaced
those with plain `!had_error` / truthiness so the learner's step-1 loop condition stayed
readable. Verified: `clang-format --dry-run -Werror` clean, focused clang-tidy clean except
the three intentional `_POSIX_C_SOURCE` feature-test macros, strict build clean, **38 tests,
5 failed** — same five red Part 6 tests, same ASan leak (each failing test returns from
`ASSERT` before `token_list_free`). No behavior changed.

### Step 2 review

Learner asked whether step 2 was ready for step 3. Read the working tree: `error_reporting`
helper exists and both unexpected-branch sites call it with `starting_offset_position` before
the cursor increment — structurally correct. Flagged that the printed line lacks the byte and
position (settle decision wanted `unexpected character '%c' at position %zu`), and that the
labels are internal-sounding. Ran scratch on `"SELECT@"`: output `error_msg: punctuation tkn
invaliderrorrrr`, NULL return, leak — confirms step 3's EOF guard / `errorrrr` removal is the
next required edit. Step 2 code is uncommitted.

**Test review (same session):** learner asked what he did wrong in the tests. Nothing is wrong
with them — the five Part 6 tests are the intended red checkpoint. The failure point moved:
before step 2 `'` was silently skipped by `default:`, so the tests reached token assertions;
now `error_reporting` fires on `'`, the `errorrrr`/NULL tail returns NULL, and every test stops
at `ASSERT_NOT_NULL`. That is step 3's target, not a test defect. One genuine gap confirmed:
`unterminated_string_reports_opening_quote` still lacks the count assertion (`total_num_tkns
== 0`) required by `current.md`. Helper on disk now takes only the position and prints
`Unexpected character '%c' at position %zu` — the byte-extraction advice landed.

**Print-location question:** learner said the step 2 error print "isn't working." Verified it is —
`./build/scratch 'SELECT@' 2>/dev/null` shows only scratch's stdout (`input : ...`), while
`2>&1 >/dev/null` shows `Unexpected character '@' at position 6` plus `errorrrr`. The line is on
**stderr by design** (Part 7 decision) so REPL output on stdout stays clean. `error_msg` remains
the static label `"Unexpected character"`; the formatted detail lives only in the printed line.

**Learner preference — keep explicit `else` branches.** Told to "keep the elses" after seeing
clang-tidy had removed them. Restored `else` in `is_at_end`, `tkn_list_add`, the `<`/`>`/`!`
lookahead branches, and `trim` in `repl.c`. `readability-else-after-return` is therefore
disabled by preference for this repo: do not re-run that check with `--fix`. Verified:
clang-format clean, strict build clean, **38 tests, 5 failed** — unchanged.

### Step 3 rolled back at learner's request

Learner clarified he wanted to write step 3 himself and asked for steps, not an
implementation. Reverted only the step 3 pieces of `d5dccfa`: restored the unconditional EOF
push plus `errorrrr` / `return NULL` tail in `src/tokenizer.c`, removed the three Part 7 tests
and the count assertion in `tests/test_tokenizer.c`, and restored `todo` / `notes/41` /
`notes/90-log.md` to their pre-step-3 text. **His step 2 work was preserved** (`error_reporting`
helper and both call sites) and is now committed as part of this rollback commit. Verified back
to **38 tests, 5 failed** — the original Part 6 red checkpoint.

### Step 3 written by learner; tests added and checked (same session)

Learner wrote the step 3 guard himself. His first build failed on a leftover
`bool add_to_list` (declared but unused under `-Werror`); removed the variable so the push is a
bare `tkn_list_add(...)` call. Wrote the three Part 7 tests and added the missing count
assertion to the unterminated test. Result: **41 tests, 4 failed** — all error tests green, only
the four valid-string tests red (step 4). Format check and strict build clean.

### Step 4 in progress — guided review (no code supplied)

Learner added `tokenize_string` at `tokenizer.c:188` and an `else if ((unsigned char)peek(&lexer)
== '\'')` arm at `:271` — placement is right. Build currently fails: `string_token` is declared
in the arm and never used. On disk the helper initializes only `token_type` and `input_string`,
never assigns `offset_position` / `word_length`, never calls `error_reporting`, and its loop
checks `peek() == '\''` starting at the opening quote. Guided by questions only, per learner
request.

**Step 4 diagnosis:** learner correctly identified that `tokenize_string`'s first loop iteration
matches the *opening* quote, so `closing_quote` becomes true immediately and the cursor never
moves. Next nudge: what must happen before the loop so the first `peek()` sees content?

### Neovim debugger "not working" — diagnosed (same session)

Checked the DAP setup itself: `dap.core` import present, headless `dap.adapters.codelldb` is
`type = "server"` with `port = "${port}"` and mason codelldb executable, and all three
`minidb:` configs are registered. So the config is not the problem. The blocker is upstream:
`make build/tests` fails on the learner's in-progress `string_token` unused-variable error, and
`lua/plugins/dap.lua`'s `program()` rebuilds with `make -C <root> build/tests` before launching.
A stray `~/.config/nvim/dap.lua` also exists, but headless inspection shows it is not winning
the adapter race on this machine.

**Quote-layer check:** learner answered `strlen("'hello'") == 7` correctly — the debugger's
display quotes and the NUL are not counted. Returned to the step 4 cursor question.

### Step 4 progress: opening quote consumed

Learner added `lexer->cursor_position += 1` before the scan loop and pushed the helper's result
in the arm. Build is clean. Suite: **41 tests, 5 failed** (all string tests). Scratch `'hello'`
shows `ERROR pos=0 len=0`, `ERROR pos=0 len=0`, `EOF pos=7`, `had_error = false` — so the helper
leaves the cursor on the closing quote, causing a second trip through the quote arm, and its
`Token` fields (`token_type`, `offset_position`, `word_length`, `input_string`) are still the
initial ones. Unterminated input never calls `error_reporting`, hence `had_error` false.

### Step 4 review: infinite loop

Learner changed the scan loop to `while (!is_at_end(lexer) && peek(lexer) != '\'')` with an
`if (peek == '\'') closing_quote = true;` body and no cursor increment; added `error_reporting`
on the failure branch and `TOKEN_STRING` on success. Build clean, but the suite **hangs**:
`script -qec 'timeout 8 ./build/tests' /dev/null` gives exit 124 and no buffered output. The
loop condition excludes the very byte the body tests for, and nothing advances the cursor.

**Learner's fix (correct):** replace `peek != '\''` in the loop condition with `!closing_quote`,
so the quote byte can enter the body and set the flag. Reminder given: the body still needs its
non-quote `else { cursor += 1; }` or the cursor never advances; `is_at_end` stays as the
unterminated-input exit.

**Learner confirmed:** when the scan loop stops, `cursor_position` points at the closing quote
(it was never consumed). Next nudge: consume it before returning, then assign the Token span.

### 2026-11-27 — token field rename applied; step 4 still red

Learner confirmed `word_length` → `token_length` (the "token length" phrasing was an
underscore question, not a new name). Renamed only the `Token` fields:
`input_string` → `token_string`, `word_length` → `token_length`, `offset_position` →
`source_index`. `Lexer.input_string` and the `tokenize(const char *input_string)` parameter
were deliberately left alone — they name the whole source, which is exactly the collision
the rename removes. Four files touched: `src/tokenizer.h`, `src/tokenizer.c`,
`tests/test_tokenizer.c`, `tests/scratch.c`; notes and `todo` updated in the same pass.

Build is clean under `-Wall -Wextra -Werror -Wshadow -Wconversion -Wpointer-arith` with
ASan/UBSan. Suite still reports **41 tests, 4 failed**, the four valid-string tests — no
behavior changed. New observation from the full run: ASan aborts in
`unterminated_string_reports_opening_quote` with a global-buffer-overflow in `peek` —
`tokenize_string` increments the cursor after its scan loop unconditionally, so the failed
scan leaves the cursor one past the NUL and the main loop reads out of bounds. Pre-existing
step-4 gap, not a rename regression; recorded in `todo`, `current.md` and `notes/41`.

### 2026-11-27 — unterminated-string cursor branch decided

Asked what should happen to `tokenize_string`'s post-loop `cursor_position += 1` on the two
loop exits. Learner answered: increment only when `closing_quote` is true. That matches the
loop's own state — the NUL exit means `had_error` is already set and the main loop terminates,
so the increment has no consumer and only produces the out-of-bounds `peek`. The fix is not yet
applied on disk; the next checkpoint is a rebuild plus the full suite, then the four valid-string
tests and the scratch inputs. Recorded in `current.md` and `notes/41`.

### 2026-11-27 — "what is an invariant?"

Learner asked for the definition mid-step-4. Answered from the code: an invariant is a condition
true at every stable point of a computation, not merely at its end; the lexer's live examples are
`cursor_position` = next unread byte and the token span invariants. Contrasted it with
`strlen(token_string) == token_length`, which was true for `"SELECT"` only by coincidence and
died on `"SELECT * FROM users"` — the distinction that makes an invariant useful as a debugging
tool rather than a description of one run. Filed under `notes/50-concepts-c.md` and ended with a
question about which invariant catches an early cursor increment.

### 2026-11-27 — invariant question landed as a gap; repaired concretely

Learner answered "im not sure" to which invariant catches an early cursor increment. That is a
genuine gap, not a wrong model: the arithmetic identity `token_length == cursor_position -
source_index` survives the bad increment, so it can't be the thing that detects it. Repair used
indices for `'hello'` (0 quote, 1-5 content, 6 quote, 7 NUL): computing the length at cursor 6
gives span `[1,6)` = `hello`; incrementing first gives `[1,7)` = `hello'`. The violated promise
is that the span is exactly the token's content, delimiters excluded. Filed in `notes/50`; next
question asks which span includes a quote byte.

### 2026-11-27 — "is the invariant the closing quote flag?"

No — and the confusion is diagnostic. `closing_quote` is a local bool that changes during the
scan, so it is a control-flow flag, not a condition true at every stable point. The invariant is
the relationship at loop exit between the flag and the cursor: true means cursor on the closing
quote, false means cursor on the NUL. Recorded in `notes/50`; next question bridges back to the
span by asking which computed length stops at the content.

### 2026-11-27 — span-length check landed

After the flag/invariant distinction and the index table, learner answered that length 5 (not 6)
stops `'hello'`'s span at the content. The invariant is now held as a usable rule: at loop exit
the cursor is the exclusive content end, so `token_length` must be computed before consuming the
closing quote. Next action is the code change in `tokenize_string` and a full-suite report.

### 2026-11-27 — stuck applying step 4

Learner reported being stuck when asked to apply the guarded increment and the span assignments
in `tokenize_string`. No code trace yet; asking which of the two edits is the break before
offering a hint, so the nudge lands at the first unanswerable point.

Localized: learner is blocked on **both** edits. Starting with the increment guard (the ASan
cause), then the field assignments; one at a time.

### 2026-11-27 — checked learner's first step-4 edit

Asked "did I make the right changes?" and inspected `src/tokenizer.c` on disk. The increment is
now inside the success branch and `token_string` targets the content — both right. Not right:
`source_index` assigned a pointer (build fails `-Wint-conversion`), `token_length` computed after
the increment and against the opening offset (7 for `'hello'` instead of 5), and a stale comment.
The dispatch arm still pushes on error. Recorded in `notes/41`; next nudge is to fix `source_index`
first and read the next compiler error.

### 2026-11-27 — second `source_index` attempt checked

Learner replaced the pointer with `lexer->input_string[starting_offset_position]` — the byte at
the opening quote (`'` = 39), not the byte's index. Build moved from `-Wint-conversion` to
`-Werror=sign-conversion`; still red. The distinction to install: `source_index` is a `size_t`
offset into the source, while `input_string[i]` is what lives at that offset. Recorded in
`notes/41`; nudge asks which expression yields the index of the first content byte.

### 2026-11-27 — `source_index` fixed; next break is `token_length`

Learner applied `source_index = starting_offset_position + 1`; build is clean. Full suite:
41 tests, 5 failed. The four valid strings are uniformly +2 (hello expected 5 got 7, empty
expected 0 got 2) because the increment now precedes the length computation and the subtraction
uses the opening offset rather than `source_index`. The unterminated test regressed to error_pos
1 because the unguarded push lets `tkn_list_add` overwrite the reported position. Recorded in
`notes/41`; next step is the length ordering/base, then the push guard.

### 2026-11-27 — length part written on request; "cognitive overload" diagnosis

Learner asked to have the length part written for him. Wrote it: compute
`token_length = cursor_position - string_token.source_index` while the cursor is still on the
closing quote, then increment; moved the stale comment with it. Verified: build clean, 41 tests
1 failed (four valid strings green; unterminated still error_pos 1), scratch `'hello'` → pos 1
len 5, `'hi'x` → STRING + IDENT, `''` → pos 1 len 0.

Asked what he was missing; he suspected cognitive overload. That is accurate at the interface
level: three distinct facts had to be held at once — (1) `source_index` is a `size_t` offset, not
the byte or pointer, (2) the cursor at loop exit is the exclusive end of content, so length comes
before the increment, (3) the length is measured from `source_index`, not the opening quote. The
load-bearing idea is (2); once it is held, (1) and (3) are bookkeeping. Recorded in `notes/41`,
`current.md` and `todo`; code + notes committed together.
