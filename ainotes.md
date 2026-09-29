# AI Notes — MiniDB (C database project)

**Purpose:** a running log of AI-assisted sessions for MiniDB, with the
clarifying questions and corrections kept in full, so context can be handed
between machines/setups without re-deriving it.

**Last updated:** 2026-09-28
**Working directory:** `/home/jcmac/code/database`

> How to use this doc when switching setups: read "Current state" and
> "Blockers" first, then "Open questions / next steps". The conversation log at
> the bottom is the *why* behind the lessons.

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

| Thing | Reality on disk (2026-09-28) |
|---|---|
| Branch | `main` |
| HEAD | `4b84f70` "cleaning" |
| `origin/main` | `4b84f70` (in sync, pushed) |
| Local `parser` | **does not exist** |
| `origin/parser` | `6f71c70` "test3" (remote only) |
| `ch01-repl` | **does not exist**, local or remote |
| `5ae9ff9` | the *skeleton* commit, 10 commits behind HEAD — not "Chapter 1 merged" |

Only **one file is uncommitted**: `src/tokenizer.c` (90 insertions / 18 deletions),
which now also contains the `token_type_name` addition from this session.

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
  TokenType type;
  const char *start;   /* points INTO the source. NOT owned. NOT NUL-terminated. */
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
  const char *source;
  size_t cursor_position;
  TokenList *lex_tkn_list;
} Lexer;
```

Public API: `tokenize`, `token_list_free`, `token_type_name`.
Header and `.c` names are now aligned.

**Design invariants to remember:**
- A `Token` does not own its text. It **points into** the caller's source, so the
  source must outlive the token list. No NUL terminator is guaranteed.
- `tokenize` returns a heap list; **the caller owns it** and calls `token_list_free`.
- `word_length` and `position` and `error_pos` are `size_t` — **unsigned**. They can
  never hold a negative sentinel (see Lesson 6).

---

## 6. Current state of `src/tokenizer.c`

**Working:**
- `peek` — clean, no debug output.
- `is_at_end` — pure yes/no.
- `list_push` — both branches now write `[total_num_tkns - 1]`; the old off-by-one
  and the backwards `!=` assert are gone.
- `token_list_free` — NULL-safe (prints "cant free" to stderr; arguably should be a
  silent no-op, mirroring `free`).
- `tokenize` no longer frees early.
- `token_type_name` — **added this session** (see §7).

**BLOCKERS — the file does not parse, so no test can run:**

```
src/tokenizer.c:183:12: error: implicit declaration of function 'is_whitespace'
                                   [-Wimplicit-function-declaration]
src/tokenizer.c:195:3:  error: expected declaration or statement at end of input
src/tokenizer.c:179:8:  error: variable 'exitStatus' set but not used
                                   [-Werror=unused-but-set-variable=]
```

1. **`is_whitespace` is called at line 183 but defined nowhere.** No `<ctype.h>`
   include either. (And per Lesson 4, this function probably should not exist.)
2. **`tokenize` is missing its closing brace.** The file ends at
   `return lexers_tkn_list;`. The restructure deleted the `}`.
3. **`exitStatus` is vestigial.** The loop used to be `while (exitStatus != true)`;
   it is now `while (!is_at_end(&lex))`, so nothing reads `exitStatus`.

**Also still broken (the actual hang bug):** the loop body is empty —

```c
while (!is_at_end(&lex)) {
  if (is_whitespace(&lex)) {
  }
}
```

— so on any real input it spins forever.

**Absent:** `advance` (and `peek_next`, `match`, `scan_number`, `scan_identifier`,
`scan_string`). See the guide's helper list at guide line ~2594.

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

### Action taken this session

- Added `token_type_name` to `src/tokenizer.c` and verified it fully with a
  standalone harness under the project's exact flags (§7). No other code changes.

### Docs worth keeping

- Crafting Interpreters, Ch. 16 "Scanning on Demand" —
  <https://craftinginterpreters.com/scanning-on-demand.html>
- `<ctype.h>` char classification (mind the `unsigned char` cast gotcha) —
  <https://en.cppreference.com/w/c/string/byte#Character_classification>
- `strncasecmp` (for case-insensitive keyword matching) —
  <https://man7.org/linux/man-pages/man3/strcasecmp.3.html>
