# 43 — `list_push` and the growth pattern

Part of the [notes tree](../ainotes.md). Read when touching token-list growth.

## The guide's requirement

> Write the growth logic **once**, in one function, using the temp-var `realloc` pattern.

Correct shape:

```c
Token *tmp = realloc(buf, new_capacity * sizeof(Token));
if (tmp == NULL) return false;   /* buf is still valid; caller decides */
buf = tmp;
capacity = new_capacity;
/* then copy the token's fields */
```

The implementation satisfies "grow logic lives in one function"
(`grow_list_capisity`), but that function returns `void`, so it reports failure only via the
`had_error` side effect (see [41-tokenizer-state](41-tokenizer-state.md) defects 3–5).

## `list_push` copies verbatim

`list_push` **does not compute** any field values. It copies `token_type`, `input_string`,
`word_length`, `start_index` **verbatim** into the next slot. Whatever is in the initializer is
what lands in the list — there is **no later "fill in real values" step**.

Consequence already hit: an EOF token written as `{TOKEN_EOF, lexer.input_string, 0,
lexer.cursor_position}` silently keeps `input_string` = start-of-input. It is not "fixed up"
later; it is inconsistent by construction.

## Related traps

- **On `realloc` failure the original block is still valid and untouched.** Never read the
  pointer after `p = realloc(p, n)` — on failure that leaks the only reference to live data.
- A function that can fail but returns `void` cannot report it.
- Detecting failure ≠ handling it: a `NULL` check that runs *after* the pointer has already
  been overwritten detects the loss but cannot recover it. The check must run **before** the
  state it guards is destroyed.
