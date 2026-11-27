#include "parser.h"

/* The parser is a cursor over tokens, one level up from the tokenizer's cursor
 * over characters. The EOF token from Chapter 2 means peek() never needs a
 * bounds check -- but parse() still rejects a hand-made empty list. */

typedef struct {
  const TokenList *tokens;
  size_t current;
  Arena *arena;
  ParseError *err;
} Parser;

/* --- cursor helpers --- */
static void set_error_raw(ParseError *err, const char *message, size_t pos);
static void set_error(Parser *foo, const char *message, size_t pos);
static const Token *peek(const Parser *foo);
static const Token *advance(Parser *foo);
static bool check(const Parser *foo, tokentype type);
static bool match(Parser *foo, tokentype type);
static bool expect(Parser *foo, tokentype type, const char *message);

/* --- statement dispatch; the grammar rules land here one at a time --- */
static Statement *statement(Parser *foo);

static void set_error_raw(ParseError *err, const char *message, size_t pos) {
  if (err != NULL) {
    err->had_error = true;
    err->error_msg = message;
    err->error_pos = pos;
  }
}

static void set_error(Parser *foo, const char *message, size_t pos) {
  set_error_raw(foo->err, message, pos);
}

static const Token *peek(const Parser *foo) {
  return &foo->tokens->token_list_buffer[foo->current];
}

static const Token *advance(Parser *foo) {
  const Token *t = peek(foo);
  if (t->token_type != TOKEN_EOF) {
    foo->current += 1;
  }
  return t;
}

static bool check(const Parser *foo, tokentype type) {
  return peek(foo)->token_type == type;
}

/* match consumes only when the type is there: for optional parts like WHERE. */
static bool match(Parser *foo, tokentype type) {
  if (!check(foo, type)) {
    return false;
  }
  (void)advance(foo);
  return true;
}

/* expect consumes or reports at the offending token: for required parts. */
static bool expect(Parser *foo, tokentype type, const char *message) {
  if (match(foo, type)) {
    return true;
  }
  set_error(foo, message, peek(foo)->source_index);
  return false;
}

static Statement *statement(Parser *foo) {
  if (match(foo, TOKEN_SELECT)) {
    set_error(foo, "SELECT rule not implemented yet", peek(foo)->source_index);
    return NULL;
  }
  if (match(foo, TOKEN_INSERT)) {
    set_error(foo, "INSERT rule not implemented yet", peek(foo)->source_index);
    return NULL;
  }
  expect(foo, TOKEN_SELECT, "expected SELECT or INSERT");
  return NULL;
}

/* Grammar rules still to add (guide 2685-2693), one function per rule:
 *   select_statement, insert_statement, column_list,
 *   expression, comparison, primary, where_clause
 * Node constructors, all arena-allocated:
 *   expr_binary, expr_column, expr_literal_int, expr_literal_str
 * Each arrives with its own failing test; no placeholder is defined until it
 * is called, so the strict build stays honest. */

Statement *parse(const TokenList *tokens, Arena *arena, ParseError *out_err) {
  ParseError local = {false, NULL, 0};
  ParseError *err = out_err != NULL ? out_err : &local;

  err->had_error = false;
  err->error_msg = NULL;
  err->error_pos = 0;

  if (tokens == NULL) {
    set_error_raw(err, "no token list", 0);
    return NULL;
  }
  if (arena == NULL) {
    set_error_raw(err, "no arena", 0);
    return NULL;
  }
  if (tokens->had_error) {
    set_error_raw(
        err, tokens->error_msg != NULL ? tokens->error_msg : "tokenizer error",
        tokens->error_pos);
    return NULL;
  }
  if (tokens->total_num_tkns == 0 || tokens->token_list_buffer == NULL) {
    set_error_raw(err, "empty token list (missing EOF)", 0);
    return NULL;
  }

  Parser foo = {tokens, 0, arena, err};
  return statement(&foo);
}
