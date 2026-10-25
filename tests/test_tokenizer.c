#define _POSIX_C_SOURCE 200809L
#include "test.h"
#include "tokenizer.h"

/* Caller owns the returned list — token_list_free() it. */
static TokenList *tokenize_with(const char *input) {
  // input = "SELECT * FROM users;"
  return tokenize(input);
}

/* [checklist] Empty input produces exactly one token: TOKEN_EOF. */
static void empty_input_is_single_eof(void) {
  TokenList *tokens_in_list = tokenize_with("");
  ASSERT_NOT_NULL(tokens_in_list);
  ASSERT_EQ_INT(1, tokens_in_list->total_num_tkns);
  ASSERT_EQ_INT(TOKEN_EOF, tokens_in_list->token_list_buffer[0].token_type);
  token_list_free(tokens_in_list);
}

/* [checklist] Whitespace-only input produces exactly one token: TOKEN_EOF. */
static void whitespace_only_input_is_single_eof(void) {
  TokenList *tknlst_buffer = tokenize_with("   \t\n");
  ASSERT_NOT_NULL(tknlst_buffer);
  ASSERT_EQ_INT(1, tknlst_buffer->total_num_tkns);
  ASSERT_EQ_INT(TOKEN_EOF, tknlst_buffer->token_list_buffer[0].token_type);
  token_list_free(tknlst_buffer);
}

/* [checklist] "SELECT" produces two tokens: TOKEN_SELECT then TOKEN_EOF. */
static void select_input_is_select_then_eof(void) {
  TokenList *tknlst = tokenize_with("SELECT");
  ASSERT_NOT_NULL(tknlst);
  ASSERT_EQ_INT(2, tknlst->total_num_tkns);

  /* the keyword: starts at index 0 and spans the 6 characters "SELECT" */
  ASSERT_EQ_INT(TOKEN_SELECT, tknlst->token_list_buffer[0].token_type);
  ASSERT_EQ_INT(0, tknlst->token_list_buffer[0].start_index);
  ASSERT_EQ_INT(6, tknlst->token_list_buffer[0].word_length);

  /* then the end-of-input marker */
  ASSERT_EQ_INT(TOKEN_EOF, tknlst->token_list_buffer[1].token_type);

  token_list_free(tknlst);
}

/* [checklist] "*" produces two tokens: TOKEN_STAR then TOKEN_EOF. */
static void star_input_is_star_then_eof(void) {
  TokenList *tknlst = tokenize_with("*");
  ASSERT_NOT_NULL(tknlst);
  ASSERT_EQ_INT(2, tknlst->total_num_tkns);

  /* the operator: one character, at index 0 */
  ASSERT_EQ_INT(TOKEN_STAR, tknlst->token_list_buffer[0].token_type);
  ASSERT_EQ_INT(0, tknlst->token_list_buffer[0].start_index);
  ASSERT_EQ_INT(1, tknlst->token_list_buffer[0].word_length);

  /* then the end-of-input marker */
  ASSERT_EQ_INT(TOKEN_EOF, tknlst->token_list_buffer[1].token_type);

  token_list_free(tknlst);
}

/* [checklist] "FROM" produces two tokens: TOKEN_FROM then TOKEN_EOF. */
static void from_input_is_from_then_eof(void) {
  TokenList *tknlst = tokenize_with("FROM");
  ASSERT_NOT_NULL(tknlst);
  ASSERT_EQ_INT(2, tknlst->total_num_tkns);

  /* the keyword: starts at index 0 and spans the 4 characters "FROM" */
  ASSERT_EQ_INT(TOKEN_FROM, tknlst->token_list_buffer[0].token_type);
  ASSERT_EQ_INT(0, tknlst->token_list_buffer[0].start_index);
  ASSERT_EQ_INT(4, tknlst->token_list_buffer[0].word_length);

  /* then the end-of-input marker */
  ASSERT_EQ_INT(TOKEN_EOF, tknlst->token_list_buffer[1].token_type);

  token_list_free(tknlst);
}

/* [checklist] case-insensitive keywords: "fRoM" is still TOKEN_FROM. */
static void from_case_variant_is_from_then_eof(void) {
  TokenList *tknlst = tokenize_with("fRoM");
  ASSERT_NOT_NULL(tknlst);
  ASSERT_EQ_INT(2, tknlst->total_num_tkns);
  ASSERT_EQ_INT(TOKEN_FROM, tknlst->token_list_buffer[0].token_type);
  ASSERT_EQ_INT(TOKEN_EOF, tknlst->token_list_buffer[1].token_type);
  token_list_free(tknlst);
}

/* [checklist] prefix guard: "fromage" is ONE TOKEN_IDENT (len 7), not
 * TOKEN_FROM — the length check must survive inside the table loop. */
static void from_prefix_is_ident_then_eof(void) {
  TokenList *tknlst = tokenize_with("fromage");
  ASSERT_NOT_NULL(tknlst);
  ASSERT_EQ_INT(2, tknlst->total_num_tkns);

  ASSERT_EQ_INT(TOKEN_IDENT, tknlst->token_list_buffer[0].token_type);
  ASSERT_EQ_INT(0, tknlst->token_list_buffer[0].start_index);
  ASSERT_EQ_INT(7, tknlst->token_list_buffer[0].word_length);
  ASSERT_EQ_INT(TOKEN_EOF, tknlst->token_list_buffer[1].token_type);

  token_list_free(tknlst);
}

/* [checklist] prefix guard (SELECT): "selection" is ONE TOKEN_IDENT (len 9) —
 * must not regress when the hard-coded if/else is replaced by the table. */
static void selection_is_ident_then_eof(void) {
  TokenList *tknlst = tokenize_with("selection");
  ASSERT_NOT_NULL(tknlst);
  ASSERT_EQ_INT(2, tknlst->total_num_tkns);

  ASSERT_EQ_INT(TOKEN_IDENT, tknlst->token_list_buffer[0].token_type);
  ASSERT_EQ_INT(0, tknlst->token_list_buffer[0].start_index);
  ASSERT_EQ_INT(9, tknlst->token_list_buffer[0].word_length);
  ASSERT_EQ_INT(TOKEN_EOF, tknlst->token_list_buffer[1].token_type);

  token_list_free(tknlst);
}

/* [checklist] position correctness / full statement: "SELECT * FROM users"
 * keeps every keyword recognized when input FOLLOWS the word — the guard for
 * the span bug (the third strncasecmp argument is word_length, NOT
 * strlen(input_string): the word is a span, the source runs past it). */
static void select_from_users_full_statement(void) {
  TokenList *tknlst = tokenize_with("SELECT * FROM users");
  ASSERT_NOT_NULL(tknlst);
  ASSERT_EQ_INT(5, tknlst->total_num_tkns);

  /* SELECT: 6 chars at index 0 */
  ASSERT_EQ_INT(TOKEN_SELECT, tknlst->token_list_buffer[0].token_type);
  ASSERT_EQ_INT(0, tknlst->token_list_buffer[0].start_index);
  ASSERT_EQ_INT(6, tknlst->token_list_buffer[0].word_length);

  /* STAR: 1 char at index 7 */
  ASSERT_EQ_INT(TOKEN_STAR, tknlst->token_list_buffer[1].token_type);
  ASSERT_EQ_INT(7, tknlst->token_list_buffer[1].start_index);
  ASSERT_EQ_INT(1, tknlst->token_list_buffer[1].word_length);

  /* FROM: 4 chars at index 9 */
  ASSERT_EQ_INT(TOKEN_FROM, tknlst->token_list_buffer[2].token_type);
  ASSERT_EQ_INT(9, tknlst->token_list_buffer[2].start_index);
  ASSERT_EQ_INT(4, tknlst->token_list_buffer[2].word_length);

  /* users: an identifier, 5 chars at index 14 */
  ASSERT_EQ_INT(TOKEN_IDENT, tknlst->token_list_buffer[3].token_type);
  ASSERT_EQ_INT(14, tknlst->token_list_buffer[3].start_index);
  ASSERT_EQ_INT(5, tknlst->token_list_buffer[3].word_length);

  /* then the end-of-input marker */
  ASSERT_EQ_INT(TOKEN_EOF, tknlst->token_list_buffer[4].token_type);

  token_list_free(tknlst);
}

/* The rest of the checklist (guide line 2610) — write these yourself:
 *   "users" -> TOKEN_IDENT length 5
 *   numbers, strings (quotes excluded), unterminated string error
 *   "<=" -> TOKEN_LTE; "<" alone -> TOKEN_LT
 *   "SELECT@" error
 *   identifiers with digits/underscores; NOT starting with digit
 *   token_list_free(NULL) is safe
 *   10,000-token stress input
 */

void suite_tokenizer(void) {
  SUITE("tokenizer");
  RUN_TEST(empty_input_is_single_eof);
  RUN_TEST(whitespace_only_input_is_single_eof);
  RUN_TEST(select_input_is_select_then_eof);
  RUN_TEST(star_input_is_star_then_eof);
  RUN_TEST(from_input_is_from_then_eof);
  RUN_TEST(from_case_variant_is_from_then_eof);
  RUN_TEST(from_prefix_is_ident_then_eof);
  RUN_TEST(selection_is_ident_then_eof);
  RUN_TEST(select_from_users_full_statement);
}
