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
  ASSERT_EQ_INT(TOKEN_EOF, tokens_in_list->tknlst_buffer[0].type);
  token_list_free(tokens_in_list);
}

/* [checklist] Whitespace-only input produces exactly one token: TOKEN_EOF. */
static void whitespace_only_input_is_single_eof(void) {
  TokenList *tokens_in_list = tokenize_with("   \t\n");
  ASSERT_NOT_NULL(tokens_in_list);
  ASSERT_EQ_INT(1, tokens_in_list->total_num_tkns);
  ASSERT_EQ_INT(TOKEN_EOF, tokens_in_list->tknlst_buffer[0].type);
  token_list_free(tokens_in_list);
}

/* The rest of the checklist (guide line 2610) — write these yourself:
 *   "SELECT" -> [SELECT, EOF]
 *   case-insensitive keywords
 *   "users" -> TOKEN_IDENT length 5
 *   "selection" -> ONE TOKEN_IDENT
 *   numbers, strings (quotes excluded), unterminated string error
 *   "<=" -> TOKEN_LTE; "<" alone -> TOKEN_LT
 *   position correctness; full statement; "SELECT@" error
 *   identifiers with digits/underscores; NOT starting with digit
 *   token_list_free(NULL) is safe
 *   10,000-token stress input
 */

void suite_tokenizer(void) {
  SUITE("tokenizer");
  RUN_TEST(empty_input_is_single_eof);
  RUN_TEST(whitespace_only_input_is_single_eof);
}
