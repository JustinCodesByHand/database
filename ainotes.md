# AI Notes — MiniDB (C database project)

**Purpose:** a running log of AI-assisted sessions for MiniDB, with the
clarifying questions and corrections kept in full, so context can be handed
between machines/setups without re-deriving it.

**Last updated:** 2026-10-03
**Working directory:** `/home/jcmac/code/database`

> How to use this doc when switching setups: read §6 "Current state" first,
> then §10 "Open questions / next steps". The conversation log at the bottom is
> the *why* behind the lessons.

> **STANDING INSTRUCTION (from the learner, 2026-10-03):** update this document
> after *every* meaningful question, discovery, or piece of progress — not in a
> batch at the end of a session. If something was learned the hard way (a
> question that was misunderstood, a bug that cost time), it gets written down
> here so the next setup does not have to rediscover it.

---

## 1. Environment / build

**This changed.** The project was previously on a Windows host driven through WSL.
It now lives on a native **CachyOS Linux** box.

| | |
|---|---|
| OS | CachyOS (Arch-based), kernel `7.2.8-1-cachyos`, x86_64 |
| Compiler | gcc 16.2.1 |
| Path | `/home/jcmac/code/database` |
| Windows path | `/mnt/c/...` **does not exist here** |

The old command:

```sh
wsl bash -lc "cd /mnt/c/Users/jcmac/database/database && make test"   # OBSOLETE on this box
```

is replaced by a plain, native:

```sh
cd /home/jcmac/code/database && make test
```

The Makefile and flags are unchanged and confirmed working:

```
CC    := gcc
CSTD  := -std=c17
WARN  := -Wall -Wextra -Werror -Wshadow -Wconversion -Wpointer-arith
DEBUG := -g3 -O0
SAN   := -fsanitize=address,undefined -fno-omit-frame-pointer
```

**Every warning is fatal** (`-Werror`). Treat a warning as a stopped build, not noise.

> Why WSL was needed once: Git Bash's gcc is MinGW and lacks POSIX
> `fmemopen`/`open_memstream`. On native Linux that constraint is gone.

---

## 2. Repo / git state

**Reality on disk as of 2026-10-03 22:25.** Re-verify with `git log --all --decorate`
before trusting this table — it has been wrong before (see the warning at the end).

| Thing | Reality |
|---|---|
| Current branch | `dev` |
| `dev` HEAD | `1d26d04` "Merge remote-tracking branch 'origin/dev' into dev" |
| `dev` vs `origin/dev` | **ahead 3** (unpushed: `80f87ec`, `9836cf4`, `1d26d04`) |
| `origin/dev` | `6338e98` "refactoring list_push" (2026-10-01) |
| `main` HEAD | `8f616df` "heavy refactor and complexity reduction" |
| `main` vs `origin/main` | ahead 2, behind 2 — diverged, needs a decision |
| Working tree | **clean** as of the incident below |
| `stash@{0}` | 2 deleted lines in `src/tokenizer.h` — **superseded, do not pop** |
| `rescue-a1ce250` | safety branch I created; see incident below |

### INCIDENT 2026-10-03 — a real commit was orphaned by detached HEAD

`a1ce250` "refactor list push" (2026-10-03 **14:48:53**) holds the entire session's
work — `ainotes.md`, `src/tokenizer.c`, `src/tokenizer.h`, `tests/test_tokenizer.c`,
**239 insertions / 126 deletions**. It matches the session diff exactly. It is **not**
reachable from any branch.

Cause, reconstructed from `git reflog --date=iso`:

| Time | Event | Effect |
|---|---|---|
| 12:22:48 | `git checkout 9836cf4...` (a **raw SHA**) | **detached HEAD**; `dev` stays at `9836cf4` |
| 14:48:53 | `commit: refactor list push` | `a1ce250` created *on the detached HEAD* |
| 14:49:24 | `checkout: moving from a1ce250... to dev` | back to branch `dev` at `9836cf4`; **`a1ce250` now unreferenced** |

The giveaway is the reflog text itself: `checkout: moving from a1ce250… to dev`
means the branch was *not* where the commit was.

**Recovered:** safety branch `rescue-a1ce250` created immediately, so `git gc` cannot
collect the object. Recovery onto `dev` is `git cherry-pick rescue-a1ce250`
(base `9836cf4` **is** an ancestor of `dev`, so the base is clean).

**Expect conflicts:** `dev` also contains `6338e98` "refactoring list_push", which
rewrote the same `src/tokenizer.c` (122 lines) and `src/tokenizer.h` (6 lines).
Both sides refactored `list_push`/`grow_list_capisity`, so `tokenizer.c` and
`tokenizer.h` will conflict and must be resolved by hand.

Also note `dev` now holds **two functionally identical merge commits** (`9836cf4`
and `1d26d04`) — at 22:22 a merge was replayed via `rebase` + `cherry-pick`. History
is messy but not broken; leave it alone unless asked to tidy.

A prior briefing claimed branch `parser`, `main` at `5ae9ff9`, and a merged
`ch01-repl` branch. All three were wrong for this checkout. **Verify git state
before trusting a cross-setup brief.**

---

## 3. Project structure

```
src/   main.c  repl.c repl.h  tokenizer.c tokenizer.h  util/hex.c util/hex.h
       minidb-c-guide.md            <- the guide; line numbers referenced throughout
tests/ test.h test_main.c test_repl.c test_tokenizer.c
Makefile
```

Everything is built together; `tests/test_tokenizer.c` is where tokenizer work is verified.

---

## 4. Chapter 1 (REPL) — DONE

- `repl.c` uses `getline`, `strcspn` to strip the newline, `trim`, then dispatch.
- `.exit` → `__EXIT`; anything else → "Cant parse sql." → `__CANTPARSE`.
- Committed and pushed. No action needed beyond a later rename (see cleanup list):
  `__EXIT` / `__CONTINUE` / `__CANTPARSE` are **reserved identifiers** (double
  underscore) — a pending cleanup.

---

## 5. Tokenizer contract (`src/tokenizer.h`)

```c
typedef struct {
  TokenType token_type;
  const char *input_string; /* points INTO the input. NOT owned. NOT NUL-terminated. */
  size_t word_length;
  size_t position;
} Token;

typedef struct {
  Token *tknlst_buffer;
  size_t total_num_tkns;
  size_t tknlst_capacity;
  bool had_error;
  const char *error_msg;
  size_t error_pos;
} TokenList;

typedef struct {
  const char *input_string;
  size_t cursor_position;
  TokenList *lex_tkn_list;
} Lexer;
```

> **WIP rename (uncommitted, working tree).** The fields were renamed:
> `Token.type` → `token_type`, `Token.start` → `input_string`,
> `Lexer.source` → `input_string`, and `tokenize`'s parameter → `input_string`.
> `tokenizer.c` has **not** been updated and still uses the old names (see §6). If this
> rename is reverted, revert these snippets too.

Public API: `tokenize`, `token_list_free`, `token_type_name`.
Header and `.c` names match **except** for the WIP field rename above.

**Design invariants to remember:**
- A `Token` does not own its text. It **points into** the caller's source, so the
  source must outlive the token list. No NUL terminator is guaranteed.
- `tokenize` returns a heap list; **the caller owns it** and calls `token_list_free`.
- `word_length` and `position` and `error_pos` are `size_t` — **unsigned**. They can
  never hold a negative sentinel (see Lesson 6).

---

## 6. Current state of `src/tokenizer.c`

**STATUS: BUILDING AND TESTS PASSING — 5 tests, 0 failed.** First green run, 2026-10-03.
No compile blockers remain.

**Naming (final, after the WIP rename):**

| field | was | now |
|---|---|---|
| enum type name | `TokenType` | `tokentype` |
| `Token.token_type` | `type` | `token_type` |
| `Token.input_string` | `start` | `input_string` |
| `Token.start_index` | `position` | `start_index` |
| `TokenList.token_list_buffer` | `tknlst_buffer` | `token_list_buffer` |
| `Lexer.input_string` | `source` | `input_string` |
| `Lexer.lex_tkn_list_struct` | `lex_tkn_list` | `lex_tkn_list_struct` |
| `tokenize` param | `source` | `input_string` |

Note: `input_string` now names **two different things** — `Token.input_string` points at a
token's first character, `Lexer.input_string` points at the whole input. They are not
interchangeable; the name is just reused.

**Functions present:**

- `peek(const Lexer *)`, `is_at_end(const Lexer *)` — fine.
- `increment_cursor(Lexer *)` — this is the guide's `advance`. **Not `static`** (the only
  helper without it) and returns `void` (the guide says `char`). Has a TODO for an over-read guard.
- `grow_list_capisity(TokenList *)` — owns the capacity math including the 0-guard.
  Returns `void`, so it *cannot* report failure.
- `add_tkn_to_tknlist(TokenList *, Token)` — copies the 4 fields.
- `list_push(TokenList *, Token)` — refactored from 3 branches + a dead fallthrough down to
  2 conditions. Good shape; the guide's "write growth logic once" instruction is satisfied.
- `token_list_free(TokenList *)` — NULL-safe and silent now (the `fprintf` is gone).
- `token_type_name(TokenType)` — all 27 cases, verified (§7).

**Open defects — none are caught by the current tests:**

1. **`tokenizer.c:138-140` — infinite loop. STILL OPEN.** The alpha loop has an **empty body**,
   so nothing advances the cursor. It also uses `|` (bitwise OR) instead of `||`: for `';'`,
   `isalpha(';') | ';'` = `0 | 59` = 59 (nonzero → true), so the condition is true for nearly
   every character. Invisible to the suite because both test inputs (empty, all-whitespace)
   never enter that loop. **This is the highest-severity item remaining.**

**Fixed 2026-10-03 (verified by inspection + green suite):**

2. ~~`tokenizer.c:47` unconditional assign~~ **FIXED.** The NULL check now lives *inside*
   `grow_list_capisity`, before the assignments. On failure neither `token_list_buffer` nor
   `tknlst_capacity` is touched, and `list_push` returns `false` before calling
   `add_tkn_to_tknlist`, so `total_num_tkns` is unchanged too. The struct stays consistent in
   every branch. Used the check-then-assign pattern (no separately named temp, but
   `new_mem_address` serves the purpose).
3. ~~`tokenizer.c:148` leak~~ **FIXED.** Now calls `token_list_free(...)` instead of
   `free(...)`, so both allocations are released.
4. ~~`tokenizer.c:37-38` irrelevant variable~~ **FIXED.** Condition is now
   `tknlst_capacity == 0` alone, and `new_capasity = 1` is a literal rather than
   `capacity + 1`.

**New open items (2026-10-03):**

5. **`token_type_name` was deleted from `tokenizer.c`.** Its declaration in the header is
   commented out (the comment-out was mangled — a stray `}` and an orphaned `*/` — since
   repaired into a deliberate note). Nothing calls it, so the build passes; any future caller
   fails at **link** time. **Recover from git: branch `dev`, commit `0efae80`** rather than
   rewriting. Remember it must be updated for the `TokenType` → `tokentype` rename.
6. **`grow_list_capisity` returns `void`.** Failure travels as a side effect (mutating
   `had_error`) rather than a return value. Works, but the function that can fail cannot report it.
7. **`had_error` is sticky — never reset.** Once true, `list_push` returns `false` for every
   subsequent push even when there is room. It is a latch, not a per-call result. It is also
   the struct's *general* error flag (the guide pairs it with `error_msg`/`error_pos` for
   tokenizer errors), so reusing it for OOM means a caller cannot distinguish "malformed SQL"
   from "out of memory". Still an open design decision.
8. **Header declared `move_cursor` but the definition is `static void increment_cursor`.**
   Different names — declared-but-undefined and defined-but-undeclared. It compiled only
   because nothing called either. The bogus declaration has been removed; a `static` function
   must not appear in a public header, and renaming the declaration to match would itself
   error ("non-static declaration follows static definition").

**Absent:** `peek_next`, `match`, `scan_number`, `scan_identifier`, `scan_string`.
---

## 7. `token_type_name` — ADDED AND VERIFIED

Implemented in `src/tokenizer.c` just before `tokenize`. Non-static (it is declared
in the header). Signature:

```c
const char *token_type_name(TokenType type);
```

**Design choices:**
- All **27** enumerators get an explicit `case`, each returning its enum-name
  spelling (`"SELECT"`, `"STAR"`, `"LTE"`, `"EOF"`, ...).
- A trailing `return "UNKNOWN";` catches values that are not real `TokenType`s.
- **No `default:`.** This is deliberate.

**Verification performed (this session):**
- Extracted the function from the repo file and built it under the exact project
  flags: compiled clean, zero warnings, ASan/UBSan on.
- All 27 names non-NULL, non-empty, and pairwise distinct.
- `token_type_name((TokenType)999)` → `"UNKNOWN"` (does not fall off the end).
- `snprintf(buf, ..., "unexpected %s", token_type_name(TOKEN_FROM))` → `"unexpected FROM"`.

**Why no `default:` — proven, not assumed:**
- Adding `TOKEN_BOOL` to the enum without a `default:` gives
  `error: enumeration value 'TOKEN_BOOL' not handled in switch [-Werror=switch]`.
- The *same* edit **with** a `default:` exits 0 and silently hands the new token `"?"`.

So the explicit 27 cases turn "forgot to name a new token type" into a build error.
That is the point.

**Open design question:** the names are enum-spellings (`"STAR"`, `"LTE"`). A future
parser may want user-facing symbols (`"*"`, `"<="`) in error messages. The guide
(around line 2863) suggests deriving `expect()` messages from `token_type_name()`,
so decide before the parser grows.

---

## 8. Lessons / concepts nailed

1. **`.` vs `->`** — value vs pointer-to-struct member access.
2. **Strings end at `'\0'`; `'\0'` vs `"\0"`** — a NUL byte vs a two-byte string.
3. **`static` = private to the translation unit vs public (header contract).**
4. **Const-correctness as a promise.** `peek` takes `const Lexer *` because it *reads*;
   `advance` cannot, because it *writes*. A function that only looks should take `const`.
   Corollary: `is_whitespace(&lex)` is redundant — it is
   `isspace((unsigned char)peek(lex))`, so don't write the wrapper.
5. **Caller-owns pattern** — `tokenize` returns a heap list; caller frees it.
6. **Unsigned fields cannot hold sentinels.** `word_length = -1` becomes `SIZE_MAX`
   (18446744073709551615). That is why `TokenList` has a separate `bool had_error`
   instead of a magic negative value.
7. **No closures / no implicit `this` in C.** `lex` is a local inside `tokenize`.
   A file-scope function like `advance` **cannot see it**; the *only* mechanism is
   passing `&lex`. This is why every helper takes `Lexer *foo`, and why the guide
   names the parameter `foo` (it is just a local name; the caller picks the object).
   Also why `lex_tkn_list` is a field **inside** the `Lexer` struct — so scanners can
   push tokens without a second parameter.
8. **ASan chronology** — allocation → free → use, and how ASan reports it.
9. **Cursor helpers** — `peek` = lookout, `advance` = walker, `is_at_end` = sentinel.
   One shared cursor, passed as `&lex`.
10. **Escape sequences** — `'\t'`, `'\n'`, `' '` are three different bytes.

### `switch` mechanics (asked explicitly this session)

- **Evaluate once.** The `switch (...)` expression is evaluated exactly one time; its
  value is saved. It is *not* re-evaluated per `case`. So `switch (peek(&lex))` calls
  `peek` one time.
- **Match, then jump.** The saved value is compared against each `case` label top to
  bottom; the first equal label wins and control **jumps there**, skipping everything
  above. Unlike `if`/`else if`, the earlier tests do **not** run.
- **Run until you stop.** From the matched label, statements flow forward until `break`,
  `return`, or `}`. Fall-through happens here and only here. `break` leaves the switch
  only — not an enclosing loop.
- **Case labels must be integer constant expressions:** `case ';'`, `case TOKEN_SELECT`,
  `case 1+2` OK. `case peek(&lex)`, `case lex.cursor_position`, `case strcmp(a,b)` are not.
  The switch *expression* may be any runtime expression; the *labels* may not.
- **Switch expression must be integer type** (`char`/`int`/`enum`), not `double`,
  struct, or string.
- **C has no case ranges.** `case '0'..'9':` is Kotlin/C#/Go/Rust, not C. Either stack
  labels (`case '0': case '1': ...`) or test before switching
  (`if (isdigit(c)) ... else switch (c) { ... }`).
- **`-Wswitch` only fires for switches on an `enum`.** `switch (peek(&lex))` on a `char`
  gets no such help; full coverage is on you.

### Traps specific to the dispatch being written

1. **Every reachable case must move the cursor.** A case that matches and returns
   without `advance()` re-enters `while (!is_at_end(&lex))` at the same char → infinite loop.
2. **`default:` for `"SELECT@"`.** Reporting the bad char and then `break`ing *without
   consuming it* loops forever on that char. Something must consume it or exit.
3. **`<ctype.h>` cast.** `isspace`/`isdigit` take an `int` that must be `unsigned char`
   or `EOF`. A negative `char` (e.g. UTF-8 byte `0xE3` → `-29`) is UB. Cast:
   `isspace((unsigned char)peek(&lex))`. Also: `<ctype.h>` is not yet included.

### `break` vs `continue` vs `advance` (asked explicitly this session)

Three different jobs — keep them separate:

- **`advance(&lex)` is the only thing that moves.** It increments `cursor_position`,
  so the lexer now points at the next character. This is the "move to the next character."
- **`break`** targets the innermost enclosing **`switch` *or* loop**. Inside a `case`, it
  exits the **switch only** — it does **not** exit the `while`.
- **`continue`** belongs **only to loops**; it cannot belong to a switch.
  `continue` outside a loop is a compile error: `continue statement not within a loop`.
  Inside a `case` it jumps to the enclosing loop's next iteration, skipping the rest of
  the switch **and** the rest of the loop body.

Demonstrated: in a `for` loop containing a `switch`, `case 2: break;` still ran the
statement *after* the switch (same iteration), while `case 3: continue;` skipped it.

**In `tokenize`, the switch is the entire body of `while (!is_at_end(&lex))`, so `break`
and `continue` are equivalent there.** `continue` is the more future-proof choice
("this iteration is done") if code is ever added after the switch.

**Critical correction:** `continue` does NOT move the cursor. If a case calls `continue`
without `advance`, the next iteration peeks the *same* character, matches the same case,
moves nothing, continues again → **infinite loop**. `advance` prevents the hang;
`break`/`continue` only prevent fall-through into the next case's body. Every
token-consuming case needs *both* a move and a terminator.

**Empty case labels are fine.** `case ' ': case '\t': case '\n':` sharing one body is
the idiom; verified to compile clean under the full flag set with no `-Wimplicit-fallthrough`
warning (nothing falls). Only a case that has *statements* and runs into the next label triggers it.

**C has no labeled `break`.** Java's `break label;` does not exist. To exit a loop from
inside a nested switch you would need a flag or `goto`.

### `realloc` / OOM / `assert` (asked explicitly this session)

- **OOM = out of memory.** `malloc`/`realloc` return `NULL` on failure and set
  `errno = ENOMEM`. They do not crash.
- **On `realloc` failure the original block is still valid and untouched.** So never
  do `p = realloc(p, n)` and then read `p`; capture into a temp, check `NULL`, then
  assign. Clobbering with `NULL` leaks the only reference to live data. (gcc's
  `-Wuse-after-free` catches the naive form.)
- **`assert(x != NULL)` aborts the program on failure** (exit 134 / SIGABRT). So the
  current `assert` in `list_push` means a memory failure **kills the database** instead
  of reporting. The guide's contract: `list_push` returns `false` on OOM.
- **Where should the failure surface?** `tokenize` returning `NULL`, or returning the
  list with `had_error = true`? Note: if `tokenize` returns `NULL`, the caller has no
  list left to read `had_error` from. **Still unanswered — decide this.**

---

### Git: detached HEAD silently orphans commits (learned 2026-10-03, the hard way)

A commit can vanish from every branch and still be perfectly intact. The mechanism:

1. `git checkout <raw-sha>` moves you to that commit **without moving any branch**.
   You are now in *detached HEAD*. `git status` says `HEAD detached at 9836cf4`.
2. You commit. The new commit is created **on the detached HEAD only**. No branch
   points at it. Nothing warns you.
3. `git checkout <branch>` returns you to the branch, which is still at the *old*
   commit. Your new commit is now **unreachable**.

Symptom: you committed, `git log` on your branch doesn't show it, and the working
tree looks like the changes vanished.

**How to spot it:** `git reflog --date=iso`. The reflog records commits that no branch
points at. If reflog shows `commit: <msg>` followed later by
`checkout: moving from <that-sha>… to <branch>`, that's exactly this.

**Prevention:**
- `git checkout <branch>` before committing. To resume a commit you already made from a
  SHA, `git checkout -b <new-branch> <sha>`.
- `git switch <branch>` is better than `git checkout` here — it refuses to leave a branch
  with uncommitted work, and its detached-HEAD message is louder.
- `git status` first line tells you immediately: `On branch dev` vs `HEAD detached at …`.
- `git config --global advice.detachedHead false` turns the warning **off** — do not do
  this. That warning is the only thing standing between you and this exact situation.

**Recovery ladder** (least destructive first):
1. `git reflog` → find the SHA. It is **not** gone; it survives until `git gc` prunes
   unreachable objects (default grace period: 2 weeks).
2. `git branch rescue-<sha> <sha>` — pins it so gc can never take it. *Do this first,
   before any experimentation.*
3. `git cherry-pick rescue-<sha>` to apply it onto your branch, or
   `git merge rescue-<sha>` if a merge is more honest about the history.
4. Last resort: `git diff <sha>~1 <sha> > /tmp/work.patch` to extract the work as a file,
   then apply it by hand.

**Never** `git reset --hard` a branch to "get the commit back" — that discards whatever
else the branch has gained. Recover *forward*, not backward.

A dangling commit is not a lost commit. Check `git reflog` before believing anything is
gone.

---

## 9. `list_push` — the growth pattern (queued cleanup)

The guide says: *write the growth logic once, in one function, with the temp-var
realloc pattern.* Currently there are two nearly-identical branches. Correct shape:

```
Token *tmp = realloc(buf, new_capacity * sizeof(Token));
if (tmp == NULL) return false;      // buf still valid; caller decides what to do
buf = tmp;
capacity = new_capacity;
// then copy the token's 4 fields
```

`list_push` **does not compute** field values. It copies `type`, `start`,
`word_length`, `position` **verbatim**. Whatever is in the initializer is what lands
in the list — there is no later "fill in real values" step.

---

## 10. Open questions / next steps (in order)

1. Resolve the three blockers so the file parses:
   - delete the `is_whitespace` call (put `isspace` inline, per Lesson 4) **or** write it;
   - restore the missing `}` closing `tokenize`;
   - do something real with `exitStatus` (or delete it).
2. Get **`empty_input_is_single_eof`** green.
3. Get **`whitespace_only_input_is_single_eof`** green (add the whitespace case).
4. Then one checklist item at a time, **new test first** (guide line ~2610):
   - `"SELECT"` → `[SELECT, EOF]`
   - case-insensitive keywords (`select`, `SeLeCt` → `TOKEN_SELECT`)
   - `"users"` → `TOKEN_IDENT`, length 5
   - `"selection"` → **one** `TOKEN_IDENT` (the length-aware keyword trap)
   - numbers; strings (quotes excluded); unterminated string → `had_error`
   - `"<="` → one `TOKEN_LTE`; `"<"` + space → one `TOKEN_LT`
   - position checks (`"SELECT * FROM"` → `FROM` at position 9)
   - `"SELECT@"` → error naming the character
   - identifiers with digits/underscores (`user_id2`), not starting with a digit
   - `token_list_free(NULL)` is safe
   - 10,000-token stress input
5. Error-ise the EOF token's fields if desired: `start = lex.source` disagrees with
   `position = lex.cursor_position`; natural value is `lex.source + lex.cursor_position`.
   (`word_length = 0` for EOF is correct, not a placeholder.)

---

## 11. Pending cleanup list (non-blocking, do later)

- [ ] Rename `__EXIT` / `__CONTINUE` / `__CANTPARSE` (reserved identifiers).
- [ ] `list_push`: single growth path, temp-var `realloc`, `return false` on OOM
      instead of `assert`.
- [ ] Decide OOM reporting: `tokenize` → `NULL` vs `had_error` on the list.
- [ ] `token_list_free(NULL)`: drop the `fprintf(stderr, "cant free")`; a safe free
      should be a silent no-op.
- [ ] Remove stale artifacts: the leftover `aaaa` comments in `tokenizer.c`.
- [ ] Decide `token_type_name` spelling: enum names vs user-facing symbols.

---

## 12. Conversation log — clarifying questions & answers

Kept because the *questions* are the reusable part; the answers are only correct for
the code state above.

### Session 2026-09-28 — "verify on disk, then what's next"

**Q: Is the brief's git/environment picture right?**
No. Brief said branch `parser`, `main` at `5ae9ff9`, a local `ch01-repl`. Disk shows
branch `main`, `main` at `4b84f70`, no `parser` locally, no `ch01-repl` at all, and
`5ae9ff9` is the skeleton commit. Also the host changed from Windows/WSL to native
CachyOS Linux, so the `wsl ... /mnt/c/...` build command is invalid here.

**Q: Which of the seven "known bugs" are actually still present?**
Five of seven were already fixed on disk (header static decl; `list_push` off-by-one;
backwards assert; `peek` debug `fprintf`; `token_list_free` NULL-safety). Still present:
the empty loop body (infinite hang), and the missing `advance` helper. Plus three
blockers the brief never mentioned — `is_whitespace` undefined, missing `}`, and
`exitStatus` unused — which are why `make test` never even ran.

**Q: Teach `switch`, not the finished code.**
Covered above in §8: evaluate-once / match-and-jump / run-until-break, what may be a
case label, no case ranges in C, `-Wswitch` only for enums.

**Q (learning check 1): Why does a whitespace predicate take a `Lexer *`?**
Learner: because it doesn't need to advance; `advance` does that.
Correction: the pointer is there to **read** the cursor, not to move it. The real
point is `const` — a look-only predicate should take `const Lexer *` as a promise.
And the conclusion is that `is_whitespace` shouldn't exist; inline `isspace`.

**Q (learning check 2): Can `case '\0':` ever run?**
Learner: no, because `peek` is checked at the top of each turn.
Correct. Sharpen: `is_at_end` is true exactly when `peek == '\0'`, so the loop guard
makes `case '\0'` dead code. This is also why `advance` can be guardless, and why EOF
is pushed *after* the loop.

**Q (learning check 3): Does `advance` need a parameter, or does it already have `lex`?**
Learner: it should be `void` and increment; unsure about access to `lex`.
Correction: `lex` is a local inside `tokenize`; a file-scope function **cannot** see it.
C has no closures/implicit `this` — passing `&lex` is the only mechanism. And the guide
wants `advance` to **return `char`** (`start = advance(lex)` captures a token's first char).

**Q (learning check 4): What is OOM?**
Explained with a runnable demo (see §8): `realloc` returns `NULL` + `ENOMEM`; the
original block survives failure; `assert` aborts (exit 134). Re-posed, still open:
where should the OOM failure surface — `tokenize` returns `NULL`, or `had_error`?

**Q (learning check 5): Is `{TOKEN_EOF, lex.source, 0, lex.cursor_position}` a placeholder?**
Learner: the `0` is a placeholder until `list_push` sets real values; maybe use `-1`.
Correction: `list_push` sets nothing — it copies verbatim, so there is no later step.
The `0` is `word_length`, and `0` is **correct** for EOF. The questionable field is
`start = lex.source` (start of input) vs `position` (end of input) — inconsistent.
`-1` is impossible anyway: `word_length` is unsigned, so `-1` becomes `SIZE_MAX`.

### Session 2026-09-29 — scope, control flow, whitespace

**Q: With no parameters, what data can `advance` see? Does it see the caller's stack?**
No. A function sees only (1) its own parameters, (2) its own locals, (3) file-scope
/`static` variables. The caller's locals are **undeclared** in the callee — the compiler
says `error: 'lex' undeclared`, not "private". The stack frames are physically adjacent
(demo showed `&lex` and `&my_own_local` 32 bytes apart) but **adjacency is not access**;
the language gives no name to reach another frame. The only two bridges are parameters
and globals. Hence `advance` must take `Lexer *foo` (non-`const`, because it writes).

**Q: Do I need to return a space token?**
No. Whitespace is **skipped**, not tokenized: (a) the enum has no whitespace `TokenType`
— the type list is the contract; (b) the `whitespace_only_input_is_single_eof` test
asserts exactly 1 token, so emitting them fails it. Whitespace still matters as a
**boundary** (it terminates a word) — you just never emit the terminator. Exception:
inside a string literal, whitespace is data and `scan_string` must keep it.

**Q: So `break` and `continue` affect the `while`, not the switch?**
Backwards. `break` targets the innermost `switch` **or** loop — in a case, it leaves the
**switch**, not the `while`. `continue` belongs only to **loops** (`continue` with no
loop = compile error) and skips to the enclosing loop's next iteration. In `tokenize`
the switch *is* the whole loop body, so both are equivalent there.

**Q: After `advance`, do I call `continue` to move to the next character?**
Almost — but **`advance` is what moves, `continue` moves nothing.** `advance` increments
`cursor_position`; `continue` only jumps back to the loop condition. `continue` without
`advance` = infinite loop on the same char. You also need `break`/`continue` to prevent
fall-through into the next case's body. Full write-up in §8.

### Session 2026-10-03 — OOM, realloc ownership, refactor review

**Q: What is OOM?**
Re-explained (had been covered 09-29; notes now point at §8 so it need not be re-derived).
OOM = "out of memory": `malloc`/`realloc` **return `NULL`** and set `errno = ENOMEM`.
They do not crash. Verified again that `assert` is **live** in this build (no `-DNDEBUG`
in the Makefile), so `assert(ptr != NULL)` turns an OOM into a SIGABRT (exit 134).
**Reconfirmed lesson:** on `realloc` failure the original block is still valid and untouched.

**Q: Do I need to reassign the new address, or does `realloc` do it automatically?**
**No — `realloc` never writes to your variable.** Demonstrated:

```
before:  buf = 0x556b4e727010
after:   buf = 0x556b4e727010   <- UNCHANGED
         tmp = 0x556b4e728040   <- what realloc returned
```

Why: `realloc(ptr, n)` receives a **copy** of the address (pass-by-value), so it has no
way to write back — same rule as `advance` not being able to see `lex`. To modify a
caller's variable you must pass the *address of* the variable; `realloc` has no such
parameter, so it returns the value instead. One channel in, one channel out.
The one-liner `p = realloc(p, n)` **leaks** on failure (proven with ASan:
`Direct leak of 16 byte(s)`). Also: on success `realloc` **frees the old block**, so
even *reading* the old pointer afterwards trips `-Wuse-after-free`. And `realloc` may
return the same address or a different one — never assume.

**Q: Can the grow function return an `int` with different codes?**
Yes, and it is one of the three legitimate C shapes. But an `int` is only one channel —
growing must convey *three* facts (new buffer, new capacity, success/failure).
`int` earns its keep at **3+ distinct outcomes**; with two outcomes `bool` is safer
because `true`/`false` cannot be misread (no `return 1;` meaning-success trap).
**Consistency argument from this file:** `list_push` and `add_tkn_to_tknlist` both take
`TokenList *` and mutate it, so grow fits that pattern and then only needs to return a
single `bool` — the three-facts problem collapses with no out-params.

**Q: Can I `switch` on capacity — `case 0` / `case full` / `default`?**
**No.** Compiler: `error: case label does not reduce to an integer constant`. Two deeper
reasons: (1) "full" is a *comparison between two runtime values* (`total == capacity`),
and a switch can only match a single value against constants — the information isn't in
the expression at all; (2) capacity has a huge value space (1,2,4,…16384), so most cases
would never match. **Rule of thumb: switch on a value from a small fixed set (a `char`, an
enum); `if` on a condition involving comparison or range.** Their `case 0` and `case full`
also overlap at (0,0). Note their `switch (peek(&lexer))` character dispatch is the
*correct* use of switch.

**Q: How does my refactor look?**
Reviewed without prescribing fixes. Praised: `list_push` went 3 branches + dead fallthrough
→ 2 conditions; the doubling rule now lives in one function as the guide demands; clean
how/when separation; `token_list_free` is now silent. Flagged four defects (§6) — the
infinite loop at 138, the too-late NULL check at 47, the leak at 148, and the irrelevant
variable in the growth condition at 37-38.

**Q (clarified after review): what exactly is wrong with #2, #3, #4?**
Explained in full below — these are the three cost the most time so far.

### New lessons from this session

- **Detecting failure ≠ handling it.** `list_push:75` checks `token_list == NULL`, but by
  then `grow_list_capisity:47` has already overwritten the only pointer to the live buffer.
  The check *detects*; it cannot *recover*. A check is only useful if it runs **before**
  the state it guards is destroyed.
- **One `malloc`, one `free`.** `TokenList` is two separate allocations (the struct, and
  the `token_list` buffer inside it). Freeing the outer one does not free the inner one.
  This is exactly what `token_list_free` exists for.
- **`|` is not `||`.** `|` evaluates both sides and does a bitwise OR; `||` short-circuits.
  In a `while`, `isalpha(c) | c` is nonzero for almost any character, so the loop condition
  is almost always true.
- **A `void` function cannot report failure.** `grow_list_capisity` returning `void` means
  `list_push` has no way to learn whether the grow worked.

### Action taken this session

- **First green test run: 5 tests, 0 failed.** (`empty_input_is_single_eof`,
  `whitespace_only_input_is_single_eof`, plus 3 repl tests.)
- Renamed test-side field access `.type` → `.token_type` to match the header/tokenizer.c.
- Learner fixed defects #2 (OOM check), #3 (`free` → `token_list_free`) and #4 (irrelevant
  variable in the growth condition) unaided; all three verified by inspection. Also made
  `increment_cursor` `static`.
- Repaired two rename-round breakages: test field `.token_list` → `.token_list_buffer`
  (2 lines), and the mangled `token_type_name` comment in the header (stray `}` + orphaned
  `*/`). Also removed the dangling `move_cursor` declaration (open item 8).
- Refreshed §6 to reflect the green build, the confirmed fixes, and the four new open items;
  added the standing instruction to keep this file current.

### New lessons (2026-10-03, later)

- **A `static` function must never appear in a public header.** And you cannot fix a
  name mismatch by renaming the declaration to match — a non-static declaration followed by a
  static definition is itself a compile error. The only correct resolution when the
  implementation is file-private is to delete the declaration.
- **A function that can fail but returns `void` cannot report the failure.** The current
  code gets the information out via a side effect (mutating `had_error`), which works but
  leaves no room to distinguish failure *kinds* — and makes the flag sticky.
- **A mangled `//` comment-out is a silent landmine.** Commenting out a line that also
  contains a brace swallows the brace and orphans the `*/`. The file still compiled, so only
  reading the header caught it.

### Docs worth keeping

- Crafting Interpreters, Ch. 16 "Scanning on Demand" —
  <https://craftinginterpreters.com/scanning-on-demand.html>
- `<ctype.h>` char classification (mind the `unsigned char` cast gotcha) —
  <https://en.cppreference.com/w/c/string/byte#Character_classification>
- `strncasecmp` (for case-insensitive keyword matching) —
  <https://man7.org/linux/man-pages/man3/strcasecmp.3.html>
