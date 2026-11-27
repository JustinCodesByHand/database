#ifndef MINIDB_PARSER_H
#define MINIDB_PARSER_H

#include <stdbool.h>
#include <stddef.h>

#include "ast.h"
#include "tokenizer.h"
#include "util/arena.h"

typedef struct {
  bool had_error;
  const char *error_msg;
  size_t error_pos;
} ParseError;

/*
 * Parses `tokens` into an AST allocated from `arena`.
 * Returns NULL on error, with details in `out_err` (which may be NULL).
 * The AST borrows string data from the original source: the source must
 * outlive both the token list and the arena.
 */
Statement *parse(const TokenList *tokens, Arena *arena, ParseError *out_err);

#endif
