# 42 — `token_type_name`

Part of the [notes tree](../ainotes.md). Read when implementing or calling `token_type_name`.

## Status: NOT currently implemented

- `src/tokenizer.c` does **not** define it.
- `src/tokenizer.h` has its declaration **commented out** (the comment-out was once mangled —
  a stray `}` and orphaned `*/` — and has been repaired into a deliberate note).
- Nothing calls it, so the build passes. Any future caller fails at **link** time.

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

## Open design question

The names are enum spellings (`"STAR"`, `"LTE"`). A future parser may want **user-facing
symbols** (`"*"`, `"<="`) in error messages. The guide (~line 2863) suggests deriving
`expect()` messages from `token_type_name()`, so **decide before the parser grows** whether
this function returns enum names or SQL symbols. See [60-open-items](60-open-items.md).
