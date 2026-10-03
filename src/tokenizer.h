#ifndef MINIDB_TOKENIZER_H
#define MINIDB_TOKENIZER_H

#include <stdbool.h>
#include <stddef.h>

typedef enum {
  TOKEN_SELECT,
  TOKEN_FROM,
  TOKEN_WHERE,
  TOKEN_INSERT,
  TOKEN_INTO,
  TOKEN_VALUES,
  TOKEN_CREATE,
  TOKEN_TABLE,
  TOKEN_DELETE,
  TOKEN_UPDATE,
  TOKEN_SET,
  TOKEN_STAR,
  TOKEN_COMMA,
  TOKEN_SEMICOLON,
  TOKEN_LPAREN,
  TOKEN_RPAREN,
  TOKEN_EQ,
  TOKEN_NEQ,
  TOKEN_LT,
  TOKEN_LTE,
  TOKEN_GT,
  TOKEN_GTE,
  TOKEN_NUMBER,
  TOKEN_STRING,
  TOKEN_IDENT,
  TOKEN_EOF,
  token_error
} tokentype;

typedef struct {
  tokentype token_type;
  const char *input_string;
  size_t word_length;
  size_t start_index;
} Token;

typedef struct {
  Token *token_list_buffer;
  size_t total_num_tkns;
  size_t tknlst_capacity;
  /* on failure: */
  bool had_error;
  const char *error_msg;
  size_t error_pos;
} TokenList;

typedef struct {
  const char *input_string;
  size_t cursor_position;
  TokenList *lex_tkn_list_struct;
} Lexer;

TokenList *tokenize(const char *input_string);

void token_list_free(TokenList *foo);

/* NOTE: `increment_cursor` is `static` in tokenizer.c, so it is file-private and
 * deliberately NOT declared here. A declaration of it in this header would also
 * conflict: non-static declaration following a static definition is an error. */

// Disabled while the `tokentype` rename settles. Restore both this declaration AND
// its definition in tokenizer.c (git: branch dev, commit 0efae80) — nothing may call
// token_type_name until both exist, or it fails at link time.
// const char *token_type_name(tokentype type); /* for error messages and tests */

#endif
