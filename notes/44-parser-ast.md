# 44 — Parser / AST skeleton

Part of the [notes tree](../ainotes.md). Read when starting or extending Chapter 3.

## Status (2026-11-27)

Skeleton is on disk and green: `src/ast.h`, `src/parser.h`, `src/parser.c`,
`src/util/arena.h`, `src/util/arena.c`, plus `tests/test_arena.c` and `tests/test_parser.c`.
**54 tests, 0 failed**, `make all` clean, ASan/UBSan clean. No grammar rule is implemented yet:
`parse` reports `"SELECT rule not implemented yet"` / `"INSERT rule not implemented yet"` for a
valid leading keyword. That is deliberate skeleton behavior, not a parser bug.

## The lifetime chain (the load-bearing fact)

`source string` outlives `TokenList` outlives `AST arena`. Every string in the AST is a
`(pointer, length)` view into the source — no copies, no ownership, no NUL at the span end.
The caller owns the arena and calls `arena_free` once. Nothing in the AST is individually freed,
so an early return on a parse error leaks nothing by construction.

## Files and why

- `ast.h`: tagged union (guide 2737-2777). `Expr` and `Statement` are plain data — the tree is
  inspectable in tests and later rewritable by the Chapter 12 planner.
- `util/arena.{h,c}`: bump allocator, `_Alignof(max_align_t)` rounding before each allocation,
  capacity checked with `remaining` so there is no overflow, `used` unchanged on failure.
  `arena_create(0)` and `arena_free(NULL)` are defined. Guide pattern at 4780-4790.
- `parser.h`: `ParseError { had_error, error_msg, error_pos }` and
  `Statement *parse(const TokenList *, Arena *, ParseError *)`.
- `parser.c`: `Parser` cursor struct; helpers `peek`, `advance`, `check`, `match`, `expect`;
  top-level `statement` dispatch.

## Name trap — already hit once

The guide writes `TokenType`; the repo enum is **`tokentype`** (`src/tokenizer.h`). The first
build failed with `unknown type name 'TokenType'`. Use the repo name. Trust disk over the guide.

## Why no placeholder rule functions yet

`-Werror=unused-function` fires both for a `static` function defined but not called **and** for a
`static` function declared but never defined. Writing all the rule stubs up front therefore fails
the strict build. The rules are listed as a comment in `parser.c` and arrive one at a time with
their own failing test. When a rule is wired up, its declaration and definition land together.

## Cursor helpers — the distinction to keep

`match` consumes only when the type is present and returns a bool (optional parts like `WHERE`).
`expect` consumes or sets `ParseError` at the offending token (required parts). `advance` never
moves past EOF. `peek` needs no bounds check because a successful `tokenize` always ends with an
EOF token; `parse` still rejects a hand-made empty list.

## Error contract at the boundary

`parse` initializes `out_err` (or a local dummy when `out_err == NULL`), then checks inputs in
this order: NULL token list → NULL arena → `tokens->had_error` (copies message and position
verbatim) → empty list. Only then does it build the `Parser`. A lexical error is the tokenizer's,
not the parser's: no rule should see it.

## Next step

Write the first failing test for `SELECT * FROM users;`: `STMT_SELECT`, table `users`,
`column_count == 0`, `where == NULL`, no error. Then add `select_statement` and the minimal
`column_list`/table/expect sequence. Guide test list at 2841-2859.
