#ifndef MINIDB_TOKENIZER_H
#define MINIDB_TOKENIZER_H

#include <stddef.h>
#include <stdbool.h>

typedef enum {
    TOKEN_SELECT, TOKEN_FROM, TOKEN_WHERE, TOKEN_INSERT, TOKEN_INTO,
    TOKEN_VALUES, TOKEN_CREATE, TOKEN_TABLE, TOKEN_DELETE, TOKEN_UPDATE, TOKEN_SET,
    TOKEN_STAR, TOKEN_COMMA, TOKEN_SEMICOLON, TOKEN_LPAREN, TOKEN_RPAREN,
    TOKEN_EQ, TOKEN_NEQ, TOKEN_LT, TOKEN_LTE, TOKEN_GT, TOKEN_GTE,
    TOKEN_NUMBER, TOKEN_STRING, TOKEN_IDENT,
    TOKEN_EOF, TOKEN_ERROR
} TokenType;

typedef struct {
    TokenType   type;
    const char *start;     /* points into the source. NOT owned. NOT NUL-terminated. */
    size_t      length;
    size_t      position;
} Token;

typedef struct {
    Token *tokens;
    size_t count;
    size_t capacity;
    /* on failure: */
    bool        had_error;
    const char *error_msg;
    size_t      error_pos;
} TokenList;

/*
 * Tokenizes `source`. The returned list points INTO `source`,
 * which must outlive the list.
 * CALLER OWNS the result — call token_list_free().
 */
TokenList *tokenize(const char *source);
void token_list_free(TokenList *foo);

const char *token_type_name(TokenType type);   /* for error messages and tests */

#endif