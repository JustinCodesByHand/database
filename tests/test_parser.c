#include "test.h"

#include "parser.h"

/* The parser checks the Chapter 2 error contract before touching any rule. */
static void tokenizer_error_reaches_parser_unchanged(void) {
  TokenList *tokens = tokenize("SELECT@");
  ASSERT_NOT_NULL(tokens);
  ASSERT(tokens->had_error);

  Arena *arena = arena_create(1024);
  ASSERT_NOT_NULL(arena);

  ParseError err = {false, NULL, 0};
  Statement *stmt = parse(tokens, arena, &err);
  ASSERT_NULL(stmt);
  ASSERT(err.had_error);
  ASSERT_EQ_STR("Unexpected character", err.error_msg);
  ASSERT_EQ_INT(6, err.error_pos);

  arena_free(arena);
  token_list_free(tokens);
}

static void parse_rejects_missing_inputs(void) {
  Arena *arena = arena_create(256);
  ASSERT_NOT_NULL(arena);
  ParseError err = {false, NULL, 0};

  ASSERT_NULL(parse(NULL, arena, &err));
  ASSERT(err.had_error);
  ASSERT_EQ_STR("no token list", err.error_msg);

  TokenList *tokens = tokenize("SELECT");
  ASSERT_NOT_NULL(tokens);
  ASSERT_NULL(parse(tokens, NULL, &err));
  ASSERT(err.had_error);
  ASSERT_EQ_STR("no arena", err.error_msg);

  token_list_free(tokens);
  arena_free(arena);
}

static void empty_statement_reports_offending_token(void) {
  TokenList *tokens = tokenize("");
  ASSERT_NOT_NULL(tokens);
  ASSERT(!tokens->had_error);

  Arena *arena = arena_create(256);
  ASSERT_NOT_NULL(arena);

  ParseError err = {false, NULL, 0};
  ASSERT_NULL(parse(tokens, arena, &err));
  ASSERT(err.had_error);
  ASSERT_EQ_STR("expected SELECT or INSERT", err.error_msg);
  ASSERT_EQ_INT(0, err.error_pos);

  arena_free(arena);
  token_list_free(tokens);
}

void suite_parser(void) {
  SUITE("Parser skeleton");

  RUN_TEST(tokenizer_error_reaches_parser_unchanged);
  RUN_TEST(parse_rejects_missing_inputs);
  RUN_TEST(empty_statement_reports_offending_token);
}
