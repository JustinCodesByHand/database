# 50 — C concepts (asked about / learned)

Part of the [notes tree](../ainotes.md). Read to recall a language concept; these are the
things that actually came up while building the lexer.

## Pointers, structs, the C model

1. **`.` vs `->`** — value vs pointer-to-struct member access.
2. **Strings end at `'\0'`; `'\0'` vs `"\0"`** — a NUL *byte* vs a two-*byte* string.
3. **`static` = private to the translation unit.** A `static` function must **never** appear in
   a public header. You also cannot fix a name mismatch by renaming the declaration to match:
   a non-static declaration followed by a static definition is itself an error. When the
   implementation is file-private, **delete the declaration** (this is what happened to
   `move_cursor`/`increment_cursor`).
4. **Const-correctness is a promise.** `peek` takes `const Lexer *` because it only *reads*;
   `advance` cannot, because it *writes*. Corollary: `is_whitespace(&lex)` is redundant — it is
   `isspace((unsigned char)peek(lex))`, so don't write the wrapper.
5. **Caller-owns pattern.** `tokenize` returns a heap list; the caller frees it with
   `token_list_free`.
6. **No closures / no implicit `this`.** `lex` is a local inside `tokenize`. A file-scope
   function cannot see it; the **only** bridges between stack frames are parameters and globals.
   That is why every helper takes `Lexer *foo` (the name `foo` is just a local name — the caller
   picks the object) and why `lex_tkn_list_struct` is a field *inside* `Lexer`.

## Types

7. **Unsigned fields cannot hold sentinels.** `word_length = -1` becomes `SIZE_MAX`
   (18446744073709551615). That is why `TokenList` has a separate `bool had_error` instead of a
   magic negative value.
8. **Escape sequences:** `'\t'`, `'\n'`, `' '` are three different bytes.

## `switch`

- **Evaluate once.** The `switch (...)` expression is evaluated exactly one time; the value is
  saved (it is *not* re-evaluated per `case`).
- **Match, then jump.** The saved value is compared against each `case` top to bottom; the first
  equal label wins and control **jumps** there, skipping everything above. Unlike `if`/`else if`,
  earlier tests do **not** run.
- **Run until you stop.** From the matched label, statements flow forward until `break`,
  `return`, or `}`. Fall-through happens only here. `break` leaves the switch only — not an
  enclosing loop.
- **Case labels must be integer constant expressions.** `case ';'`, `case TOKEN_SELECT`,
  `case 1+2` are OK. `case peek(&lex)`, `case lex.cursor_position`, `case strcmp(a,b)` are not.
  The switch *expression* may be any runtime expression; the *labels* may not.
- **Switch expression must be integer type** (`char`/`int`/`enum`), not `double`, struct, or
  string.
- **C has no case ranges.** `case '0'..'9':` is Kotlin/C#/Go/Rust — not C. Stack labels
  (`case '0': case '1': ...`) or test before switching.
- **`-Wswitch` only fires for switches on an `enum`.** `switch (peek(&lex))` on a `char` gets
  no coverage help; full coverage is on you.
- **Switch on a small fixed set** (a `char`, an enum); use `if` for comparisons/ranges. A switch
  cannot match "full" (`total == capacity`) because that is a comparison between two runtime
  values — the info isn't in the expression.

## `sizeof` and arrays

- **`sizeof X` is bytes — nothing else.** `sizeof KEYWORDS` = the whole array's bytes
  (11 × 16 = 176 on this machine); `sizeof KEYWORDS[0]` = one entry's (16). The array-count
  idiom `sizeof arr / sizeof arr[0]` is *whole ÷ one = how many* — plain arithmetic, not
  special syntax. It exists so the loop's count can never disagree with the table.
- **The idiom works only where the compiler sees the actual array** (same file). Passed to a
  function, an array arrives as a bare pointer and `sizeof` reports 8. That is why the
  keyword table and its loop both live in `tokenizer.c`.
- **A struct is padded to a multiple of its largest member's alignment** —
  `keyword {const char *text; tokentype type;}` is 8 + 4 = 12, padded to 16 (verified with a
  scratch run, not assumed).

## Loops and control flow: `advance` vs `break` vs `continue`

- **The `for` line is three slots: start / keep-going test / step.** `size_t i = 0` runs
  once; `i < n` runs before every pass; `i++` runs after every pass — the *only* thing that
  moves `i`. Nothing else touches it.
- **`arr[i]` is a read, not a mover.** The array never moves; the index moves; indexing
  selects by number. On each pass `i` names which entry to read — the entry never points
  back.

- **`advance`/`increment_cursor` is the only thing that moves the cursor.** `continue` moves
  nothing.
- **`break`** targets the innermost enclosing **`switch` or loop**. Inside a `case` it exits the
  **switch only** — not the `while`.
- **`continue`** belongs **only to loops**; `continue` outside a loop is a compile error. Inside
  a `case` it jumps to the enclosing loop's next iteration, skipping the rest of the switch *and*
  the rest of the loop body.
- In `tokenize`, the switch *is* the whole `while` body, so `break` and `continue` are
  equivalent there. `continue` is the more future-proof choice.
- **Empty case labels are fine:** `case ' ': case '\t': case '\n':` sharing one body is the
  idiom; verified to compile clean under the full flag set with no `-Wimplicit-fallthrough`
  warning (nothing falls). Only a case with *statements* running into the next label triggers it.
- **C has no labeled `break`.** Java's `break label;` does not exist. To exit a loop from inside
  a nested switch you need a flag or `goto`.

## Traps specific to a character-dispatch lexer

1. **Every reachable case must move the cursor.** A case that matches and returns without
   advancing re-enters the loop at the same char → infinite loop.
2. This is not only about `case`s: the *alpha loop* itself must advance, and the code **after**
   the loop must consume something even when the char matched nothing. A dispatch that only
   handles the characters it knows will hang on the rest.
3. **`default:` handling must consume the bad character** (or exit), or it loops forever.
4. **`<ctype.h>` cast:** `isspace`/`isdigit` take an `int` that must be `unsigned char` or
   `EOF`. A negative `char` (UTF-8 byte `0xE3` → `-29`) is UB. Cast:
   `isspace((unsigned char)peek(&lex))`.

## `realloc`, OOM, `assert`

- **OOM = out of memory.** `malloc`/`realloc` return `NULL` and set `errno = ENOMEM`; they do not
  crash.
- **On `realloc` failure the original block is still valid and untouched.** Never
  `p = realloc(p, n)` then read `p`; capture into a temp, check `NULL`, then assign. Clobbering
  with `NULL` leaks the only reference to live data (proven with ASan: `Direct leak of 16
  byte(s)`). gcc's `-Wuse-after-free` catches the naive form. On success `realloc` **frees the
  old block**, so even *reading* the old pointer afterwards trips `-Wuse-after-free`. It may
  return the same address or a different one — never assume.
- **`realloc` never writes back to your variable** — it receives a *copy* of the address
  (pass-by-value), exactly like `advance` cannot see `lex`. One channel in, one channel out.
- **`assert(x != NULL)` aborts (exit 134 / SIGABRT).** `assert` is **live** in this build (no
  `-DNDEBUG`), so an `assert` on a memory result turns an OOM into a dead database instead of a
  reported error. The guide's contract for `list_push` is `return false`.
- **Open question:** where should the OOM failure surface — `tokenize` returns `NULL`, or
  returns the list with `had_error = true`? If `tokenize` returns `NULL`, the caller has no list
  to read `had_error` from. Still undecided — see [60-open-items](60-open-items.md).
- **An `int` return earns its keep at 3+ distinct outcomes**; with two, `bool` is safer (no
  `return 1;` meaning-success trap). Growing must convey *three* facts (new buffer, new
  capacity, success/failure) — but if the function already takes `TokenList *` and mutates it,
  only success/failure remains, so a single `bool` is enough.

## Process lessons (cost real time)

- **Detecting failure ≠ handling it.** A `NULL` check that runs after the pointer was
  overwritten detects the loss but cannot recover it. Checks must run **before** the state they
  guard is destroyed.
- **One `malloc`, one `free`.** `TokenList` is two allocations (the struct and its buffer).
  Freeing the outer one does not free the inner one.
- **`|` is not `||`.** `|` evaluates both sides and does a bitwise OR; `||` short-circuits. In a
  loop, `isalpha(c) | c` is nonzero for almost any character, so the condition is almost always
  true.
- **A `void` function cannot report failure.** Getting the info out via a side effect works but
  makes the flag sticky and hides failure kinds.
- **A loop that never changes what it examines can never terminate.** Look → decide → **move**.
- **A mangled `//` comment-out is a silent landmine.** Commenting out a line that also contains a
  brace swallows the brace and orphans the `*/`. The file still compiled, so only reading it
  caught the problem.

## ASan

- Order is allocation → free → use; ASan reports use-after-free and leaks.
- **A leak report after a failed test is a symptom, not a bug.** `assert` aborts *before*
  `token_list_free`, so the "leak" is just the abandoned allocation. Read the assertion first.
- **`abort()` discards block-buffered stdout when piped**, so `make test` can show only the leak
  report and no test output. Use `script -qec ./build/tests /dev/null` for a pty.
- **`stdbuf` conflicts with ASan's `LD_PRELOAD`** ("ASan runtime does not come first in initial
  library list"). Use `script`, not `stdbuf`.

## Docs worth keeping

- Crafting Interpreters, Ch. 16 "Scanning on Demand" —
  <https://craftinginterpreters.com/scanning-on-demand.html>
- `<ctype.h>` character classification (mind the `unsigned char` cast) —
  <https://en.cppreference.com/w/c/string/byte#Character_classification>
- `strncasecmp` (case-insensitive keyword matching) —
  <https://man7.org/linux/man-pages/man3/strcasecmp.3.html>
