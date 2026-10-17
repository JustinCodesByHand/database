# 40 — Tokenizer contract (`src/tokenizer.h`)

Part of the [notes tree](../ainotes.md). Read before editing the tokenizer: this is the
contract the rest of the code depends on.

## Types (as they are on disk, 2026-10-16)

```c
typedef enum {
  TOKEN_SELECT, TOKEN_FROM, TOKEN_WHERE, TOKEN_INSERT, TOKEN_INTO, TOKEN_VALUES,
  TOKEN_CREATE, TOKEN_TABLE, TOKEN_DELETE, TOKEN_UPDATE, TOKEN_SET,
  TOKEN_STAR, TOKEN_COMMA, TOKEN_SEMICOLON, TOKEN_LPAREN, TOKEN_RPAREN,
  TOKEN_EQ, TOKEN_NEQ, TOKEN_LT, TOKEN_LTE, TOKEN_GT, TOKEN_GTE,
  TOKEN_NUMBER, TOKEN_STRING, TOKEN_IDENT, TOKEN_EOF,
  token_error
} tokentype;                     /* 26 real tokens + token_error = 27 enumerators */

typedef struct {
  tokentype token_type;
  const char *input_string;      /* points INTO the input. NOT owned. NOT NUL-terminated. */
  size_t word_length;
  size_t start_index;
} Token;

typedef struct {
  Token *token_list_buffer;
  size_t total_num_tkns;
  size_t tknlst_capacity;
  bool had_error;
  const char *error_msg;
  size_t error_pos;
} TokenList;

typedef struct {
  const char *input_string;
  size_t cursor_position;
  TokenList *lex_tkn_list_struct;
} Lexer;
```

Public API: `tokenize`, `token_list_free`. `token_type_name` is currently **not declared**
(commented out) — see [42-token-type-name](42-token-type-name.md).

## Naming — the rename is finished

An earlier note called the field rename a "WIP rename (uncommitted)". It is now **done and
committed**. Final names:

| Thing | old | now |
|---|---|---|
| enum type name | `TokenType` | `tokentype` |
| `Token.token_type` | `type` | `token_type` |
| `Token.input_string` | `start` | `input_string` |
| `Token.start_index` | `position` | `start_index` |
| `TokenList.token_list_buffer` | `tknlst_buffer` | `token_list_buffer` |
| `TokenList.tknlst_capacity` | `capacity` | `tknlst_capacity` (kept the old spelling) |
| `Lexer.input_string` | `source` | `input_string` |
| `Lexer.lex_tkn_list_struct` | `lex_tkn_list` | `lex_tkn_list_struct` |
| `tokenize` param | `source` | `input_string` |

**Gotcha:** the name `input_string` now means two different things —
`Token.input_string` points at a token's **first character**; `Lexer.input_string` points at
the **whole input**. They are not interchangeable.

## Invariants to remember

- **A `Token` does not own its text.** It points *into* the caller's source, so the source
  must outlive the token list. No NUL terminator is guaranteed.
- **`tokenize` returns a heap list; the caller owns it** and must call `token_list_free`.
- **`word_length`, `start_index`, `error_pos` are `size_t` (unsigned).** They can never hold a
  negative sentinel — `-1` becomes `SIZE_MAX`. That is why `TokenList` has a separate
  `bool had_error` instead of a magic negative.
- **A token is a span, not a copy.** `word_length = cursor_position - start_index`, and
  `cursor_position` is exclusive (points one past the last consumed char).
