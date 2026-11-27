# 42 — `token_type_name`

Part of the [notes tree](../ainotes.md). Read when implementing or calling `token_type_name`.

## Status: IMPLEMENTED 2026-11-27

- `src/tokenizer.c` defines all 27 cases plus the trailing fallback.
- `src/tokenizer.h` declares `const char *token_type_name(tokentype type);`.
- `tests/scratch.c` calls the real function; the duplicate `scratch_type_name` is deleted.
- Test `token_type_name_maps_known_and_unknown` pins SELECT, EOF and `(tokentype)999`.

## It is NOT lost — recover it from `tests/scratch.c`

An earlier claim that "no commit contains the body, treat it as a rewrite" was **wrong**. The
complete 27-case body survives verbatim in `tests/scratch.c` as `scratch_type_name` (around
line 25). The literal string `token_type_name` appears in that file only in a **comment**, so a
`git grep token_type_name` never found the function.

**Lesson:** grepping for a *name* cannot find a function that was renamed. Search for the
**shape** (a `switch` over `tokentype`), not the name.

### To implement it

1. Rename `scratch_type_name` → `token_type_name`.
2. Move it from `tests/scratch.c` into `src/tokenizer.c` (just before `tokenize`).
3. Fix `case TOKEN_ERROR:` → `case token_error:` (the enum value is lowercase).
4. Uncomment the declaration in `src/tokenizer.h`.
5. Delete the scratch copy and call the real one.

## Design (what the verified implementation did)

- Signature `const char *token_type_name(tokentype type);`
- All **27** enumerators get an explicit `case`, each returning its enum-name spelling
  (`"SELECT"`, `"STAR"`, `"LTE"`, `"EOF"`, ...).
- A trailing `return "UNKNOWN";` catches values that are not real `tokentype`s.
- **No `default:`** — deliberate.

### Why no `default:` (proven, not assumed)

Compiler rule verified again 2026-11-27 with a three-enum scratch file under
`-Wall -Wextra -Werror`: a switch handling only `A` and `B` fails with
`error: enumeration value 'C' not handled in switch [-Werror=switch]`; adding `default:` makes the
same file compile clean. `-Wswitch` is an enum-coverage check, and `default:` suppresses it. The
trailing fallback after the switch still handles a bogus `(tokentype)999` without giving up that
check.

- Adding `TOKEN_BOOL` to the enum *without* a `default:` gives
  `error: enumeration value 'TOKEN_BOOL' not handled in switch [-Werror=switch]`.
- The *same* edit **with** a `default:` exits 0 and silently hands the new token `"?"`.

So the explicit 27 cases turn "forgot to name a new token type" into a **build error**. That is
the point.

### Verification performed when it last existed

- Extracted and built under the exact project flags: clean, zero warnings, ASan/UBSan on.
- All 27 names non-NULL, non-empty, pairwise distinct.
- `token_type_name((tokentype)999)` → `"UNKNOWN"` (does not fall off the end).
- `snprintf(buf, ..., "unexpected %s", token_type_name(TOKEN_FROM))` → `"unexpected FROM"`.

## Design decision closed 2026-11-27

Enum spellings (`"STAR"`, `"LTE"`) were chosen and documented in `src/tokenizer.h`. The parser's
`expect()` messages will use those names; display symbols were considered but not adopted.
