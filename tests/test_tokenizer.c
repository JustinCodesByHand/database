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
  const char *input = "SELECT";
  TokenList *tknlst = tokenize_with(input);
  ASSERT_NOT_NULL(tknlst);
  ASSERT_EQ_INT(2, tknlst->total_num_tkns);

  /* the keyword: starts at index 0 and spans the 6 characters "SELECT" */
  ASSERT_EQ_INT(TOKEN_SELECT, tknlst->token_list_buffer[0].token_type);
  ASSERT_EQ_INT(0, tknlst->token_list_buffer[0].source_index);
  ASSERT_EQ_INT(6, tknlst->token_list_buffer[0].token_length);

  /* then the end-of-input marker: a zero-length span at the NUL */
  ASSERT_EQ_INT(TOKEN_EOF, tknlst->token_list_buffer[1].token_type);
  ASSERT_EQ_INT(6, tknlst->token_list_buffer[1].source_index);
  ASSERT_EQ_INT(0, tknlst->token_list_buffer[1].token_length);
  ASSERT(tknlst->token_list_buffer[1].token_string == input + 6);

  token_list_free(tknlst);
}

/* [checklist] "*" produces two tokens: TOKEN_STAR then TOKEN_EOF. */
static void star_input_is_star_then_eof(void) {
  TokenList *tknlst = tokenize_with("*");
  ASSERT_NOT_NULL(tknlst);
  ASSERT_EQ_INT(2, tknlst->total_num_tkns);

  /* the operator: one character, at index 0 */
  ASSERT_EQ_INT(TOKEN_STAR, tknlst->token_list_buffer[0].token_type);
  ASSERT_EQ_INT(0, tknlst->token_list_buffer[0].source_index);
  ASSERT_EQ_INT(1, tknlst->token_list_buffer[0].token_length);

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
  ASSERT_EQ_INT(0, tknlst->token_list_buffer[0].source_index);
  ASSERT_EQ_INT(4, tknlst->token_list_buffer[0].token_length);

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
  ASSERT_EQ_INT(0, tknlst->token_list_buffer[0].source_index);
  ASSERT_EQ_INT(7, tknlst->token_list_buffer[0].token_length);
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
  ASSERT_EQ_INT(0, tknlst->token_list_buffer[0].source_index);
  ASSERT_EQ_INT(9, tknlst->token_list_buffer[0].token_length);
  ASSERT_EQ_INT(TOKEN_EOF, tknlst->token_list_buffer[1].token_type);

  token_list_free(tknlst);
}

/* [checklist] position correctness / full statement: "SELECT * FROM users"
 * keeps every keyword recognized when input FOLLOWS the word — the guard for
 * the span bug (the third strncasecmp argument is token_length, NOT
 * strlen(token_string): the word is a span, the source runs past it). */
static void select_from_users_full_statement(void) {
  TokenList *tknlst = tokenize_with("SELECT * FROM users");
  ASSERT_NOT_NULL(tknlst);
  ASSERT_EQ_INT(5, tknlst->total_num_tkns);

  /* SELECT: 6 chars at index 0 */
  ASSERT_EQ_INT(TOKEN_SELECT, tknlst->token_list_buffer[0].token_type);
  ASSERT_EQ_INT(0, tknlst->token_list_buffer[0].source_index);
  ASSERT_EQ_INT(6, tknlst->token_list_buffer[0].token_length);

  /* STAR: 1 char at index 7 */
  ASSERT_EQ_INT(TOKEN_STAR, tknlst->token_list_buffer[1].token_type);
  ASSERT_EQ_INT(7, tknlst->token_list_buffer[1].source_index);
  ASSERT_EQ_INT(1, tknlst->token_list_buffer[1].token_length);

  /* FROM: 4 chars at index 9 */
  ASSERT_EQ_INT(TOKEN_FROM, tknlst->token_list_buffer[2].token_type);
  ASSERT_EQ_INT(9, tknlst->token_list_buffer[2].source_index);
  ASSERT_EQ_INT(4, tknlst->token_list_buffer[2].token_length);

  /* users: an identifier, 5 chars at index 14 */
  ASSERT_EQ_INT(TOKEN_IDENT, tknlst->token_list_buffer[3].token_type);
  ASSERT_EQ_INT(14, tknlst->token_list_buffer[3].source_index);
  ASSERT_EQ_INT(5, tknlst->token_list_buffer[3].token_length);

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

/* [Part 1] "," is TOKEN_COMMA. */
static void comma_is_comma_then_eof(void) {
  TokenList *l = tokenize_with(",");
  ASSERT_NOT_NULL(l);
  ASSERT_EQ_INT(2, l->total_num_tkns);
  ASSERT_EQ_INT(TOKEN_COMMA, l->token_list_buffer[0].token_type);
  ASSERT_EQ_INT(0, l->token_list_buffer[0].source_index);
  ASSERT_EQ_INT(1, l->token_list_buffer[0].token_length);
  ASSERT_EQ_INT(TOKEN_EOF, l->token_list_buffer[1].token_type);
  token_list_free(l);
}

/* [Part 1] ";" is TOKEN_SEMICOLON. */
static void semicolon_is_semicolon_then_eof(void) {
  TokenList *l = tokenize_with(";");
  ASSERT_NOT_NULL(l);
  ASSERT_EQ_INT(2, l->total_num_tkns);
  ASSERT_EQ_INT(TOKEN_SEMICOLON, l->token_list_buffer[0].token_type);
  ASSERT_EQ_INT(0, l->token_list_buffer[0].source_index);
  ASSERT_EQ_INT(1, l->token_list_buffer[0].token_length);
  ASSERT_EQ_INT(TOKEN_EOF, l->token_list_buffer[1].token_type);
  token_list_free(l);
}

/* [Part 1] "(" is TOKEN_LPAREN. */
static void lparen_is_lparen_then_eof(void) {
  TokenList *l = tokenize_with("(");
  ASSERT_NOT_NULL(l);
  ASSERT_EQ_INT(2, l->total_num_tkns);
  ASSERT_EQ_INT(TOKEN_LPAREN, l->token_list_buffer[0].token_type);
  ASSERT_EQ_INT(0, l->token_list_buffer[0].source_index);
  ASSERT_EQ_INT(1, l->token_list_buffer[0].token_length);
  ASSERT_EQ_INT(TOKEN_EOF, l->token_list_buffer[1].token_type);
  token_list_free(l);
}

/* [Part 1] ")" is TOKEN_RPAREN. */
static void rparen_is_rparen_then_eof(void) {
  TokenList *l = tokenize_with(")");
  ASSERT_NOT_NULL(l);
  ASSERT_EQ_INT(2, l->total_num_tkns);
  ASSERT_EQ_INT(TOKEN_RPAREN, l->token_list_buffer[0].token_type);
  ASSERT_EQ_INT(0, l->token_list_buffer[0].source_index);
  ASSERT_EQ_INT(1, l->token_list_buffer[0].token_length);
  ASSERT_EQ_INT(TOKEN_EOF, l->token_list_buffer[1].token_type);
  token_list_free(l);
}

/* [Part 1] "a,b" -> IDENT, COMMA, IDENT, EOF (4 tokens). */
static void ident_comma_ident(void) {
  TokenList *l = tokenize_with("a,b");
  ASSERT_NOT_NULL(l);
  ASSERT_EQ_INT(4, l->total_num_tkns);

  ASSERT_EQ_INT(TOKEN_IDENT, l->token_list_buffer[0].token_type);
  ASSERT_EQ_INT(0, l->token_list_buffer[0].source_index);
  ASSERT_EQ_INT(1, l->token_list_buffer[0].token_length);

  ASSERT_EQ_INT(TOKEN_COMMA, l->token_list_buffer[1].token_type);
  ASSERT_EQ_INT(1, l->token_list_buffer[1].source_index);
  ASSERT_EQ_INT(1, l->token_list_buffer[1].token_length);

  ASSERT_EQ_INT(TOKEN_IDENT, l->token_list_buffer[2].token_type);
  ASSERT_EQ_INT(2, l->token_list_buffer[2].source_index);
  ASSERT_EQ_INT(1, l->token_list_buffer[2].token_length);

  ASSERT_EQ_INT(TOKEN_EOF, l->token_list_buffer[3].token_type);
  token_list_free(l);
}

/* [Part 2] "=" is TOKEN_EQ. */
static void eq_is_eq_then_eof(void) {
  TokenList *l = tokenize_with("=");
  ASSERT_NOT_NULL(l);
  ASSERT_EQ_INT(2, l->total_num_tkns);
  ASSERT_EQ_INT(TOKEN_EQ, l->token_list_buffer[0].token_type);
  ASSERT_EQ_INT(0, l->token_list_buffer[0].source_index);
  ASSERT_EQ_INT(1, l->token_list_buffer[0].token_length);
  ASSERT_EQ_INT(TOKEN_EOF, l->token_list_buffer[1].token_type);
  token_list_free(l);
}

/* [Part 2] "<" is TOKEN_LT. */
static void lt_is_lt_then_eof(void) {
  TokenList *l = tokenize_with("<");
  ASSERT_NOT_NULL(l);
  ASSERT_EQ_INT(2, l->total_num_tkns);
  ASSERT_EQ_INT(TOKEN_LT, l->token_list_buffer[0].token_type);
  ASSERT_EQ_INT(0, l->token_list_buffer[0].source_index);
  ASSERT_EQ_INT(1, l->token_list_buffer[0].token_length);
  ASSERT_EQ_INT(TOKEN_EOF, l->token_list_buffer[1].token_type);
  token_list_free(l);
}

/* [Part 2] ">" is TOKEN_GT. */
static void gt_is_gt_then_eof(void) {
  TokenList *l = tokenize_with(">");
  ASSERT_NOT_NULL(l);
  ASSERT_EQ_INT(2, l->total_num_tkns);
  ASSERT_EQ_INT(TOKEN_GT, l->token_list_buffer[0].token_type);
  ASSERT_EQ_INT(0, l->token_list_buffer[0].source_index);
  ASSERT_EQ_INT(1, l->token_list_buffer[0].token_length);
  ASSERT_EQ_INT(TOKEN_EOF, l->token_list_buffer[1].token_type);
  token_list_free(l);
}

/* [Part 2] "a>b" -> IDENT, GT, IDENT, EOF. */
static void ident_gt_ident(void) {
  TokenList *l = tokenize_with("a>b");
  ASSERT_NOT_NULL(l);
  ASSERT_EQ_INT(4, l->total_num_tkns);

  ASSERT_EQ_INT(TOKEN_IDENT, l->token_list_buffer[0].token_type);
  ASSERT_EQ_INT(0, l->token_list_buffer[0].source_index);
  ASSERT_EQ_INT(1, l->token_list_buffer[0].token_length);

  ASSERT_EQ_INT(TOKEN_GT, l->token_list_buffer[1].token_type);
  ASSERT_EQ_INT(1, l->token_list_buffer[1].source_index);
  ASSERT_EQ_INT(1, l->token_list_buffer[1].token_length);

  ASSERT_EQ_INT(TOKEN_IDENT, l->token_list_buffer[2].token_type);
  ASSERT_EQ_INT(2, l->token_list_buffer[2].source_index);
  ASSERT_EQ_INT(1, l->token_list_buffer[2].token_length);

  ASSERT_EQ_INT(TOKEN_EOF, l->token_list_buffer[3].token_type);
  token_list_free(l);
}

/* [Part 3] "<=" is TOKEN_LTE, len 2. */
static void lte_twochar(void) {
  TokenList *l = tokenize_with("<=");
  ASSERT_NOT_NULL(l);
  ASSERT_EQ_INT(2, l->total_num_tkns);
  ASSERT_EQ_INT(TOKEN_LTE, l->token_list_buffer[0].token_type);
  ASSERT_EQ_INT(0, l->token_list_buffer[0].source_index);
  ASSERT_EQ_INT(2, l->token_list_buffer[0].token_length);
  ASSERT_EQ_INT(TOKEN_EOF, l->token_list_buffer[1].token_type);
  token_list_free(l);
}

/* [Part 3] ">=" is TOKEN_GTE, len 2. */
static void gte_twochar(void) {
  TokenList *l = tokenize_with(">=");
  ASSERT_NOT_NULL(l);
  ASSERT_EQ_INT(2, l->total_num_tkns);
  ASSERT_EQ_INT(TOKEN_GTE, l->token_list_buffer[0].token_type);
  ASSERT_EQ_INT(0, l->token_list_buffer[0].source_index);
  ASSERT_EQ_INT(2, l->token_list_buffer[0].token_length);
  ASSERT_EQ_INT(TOKEN_EOF, l->token_list_buffer[1].token_type);
  token_list_free(l);
}

/* [Part 3] "!=" is TOKEN_NEQ, len 2. */
static void neq_twochar(void) {
  TokenList *l = tokenize_with("!=");
  ASSERT_NOT_NULL(l);
  ASSERT_EQ_INT(2, l->total_num_tkns);
  ASSERT_EQ_INT(TOKEN_NEQ, l->token_list_buffer[0].token_type);
  ASSERT_EQ_INT(0, l->token_list_buffer[0].source_index);
  ASSERT_EQ_INT(2, l->token_list_buffer[0].token_length);
  ASSERT_EQ_INT(TOKEN_EOF, l->token_list_buffer[1].token_type);
  token_list_free(l);
}

/* [Part 3] trap: "<" alone is still TOKEN_LT (len 1). */
static void lt_single_still_lt(void) {
  TokenList *l = tokenize_with("<");
  ASSERT_NOT_NULL(l);
  ASSERT_EQ_INT(2, l->total_num_tkns);
  ASSERT_EQ_INT(TOKEN_LT, l->token_list_buffer[0].token_type);
  ASSERT_EQ_INT(0, l->token_list_buffer[0].source_index);
  ASSERT_EQ_INT(1, l->token_list_buffer[0].token_length);
  ASSERT_EQ_INT(TOKEN_EOF, l->token_list_buffer[1].token_type);
  token_list_free(l);
}

/* [Part 3] trap: "a<b" -> IDENT, LT, IDENT, EOF. */
static void ident_lt_ident(void) {
  TokenList *l = tokenize_with("a<b");
  ASSERT_NOT_NULL(l);
  ASSERT_EQ_INT(4, l->total_num_tkns);

  ASSERT_EQ_INT(TOKEN_IDENT, l->token_list_buffer[0].token_type);
  ASSERT_EQ_INT(0, l->token_list_buffer[0].source_index);
  ASSERT_EQ_INT(1, l->token_list_buffer[0].token_length);

  ASSERT_EQ_INT(TOKEN_LT, l->token_list_buffer[1].token_type);
  ASSERT_EQ_INT(1, l->token_list_buffer[1].source_index);
  ASSERT_EQ_INT(1, l->token_list_buffer[1].token_length);

  ASSERT_EQ_INT(TOKEN_IDENT, l->token_list_buffer[2].token_type);
  ASSERT_EQ_INT(2, l->token_list_buffer[2].source_index);
  ASSERT_EQ_INT(1, l->token_list_buffer[2].token_length);

  ASSERT_EQ_INT(TOKEN_EOF, l->token_list_buffer[3].token_type);
  token_list_free(l);
}

/* [Part 3] trap: "a<=b" -> IDENT, LTE, IDENT, EOF. */
static void ident_lte_ident(void) {
  TokenList *l = tokenize_with("a<=b");
  ASSERT_NOT_NULL(l);
  ASSERT_EQ_INT(4, l->total_num_tkns);

  ASSERT_EQ_INT(TOKEN_IDENT, l->token_list_buffer[0].token_type);
  ASSERT_EQ_INT(0, l->token_list_buffer[0].source_index);
  ASSERT_EQ_INT(1, l->token_list_buffer[0].token_length);

  ASSERT_EQ_INT(TOKEN_LTE, l->token_list_buffer[1].token_type);
  ASSERT_EQ_INT(1, l->token_list_buffer[1].source_index);
  ASSERT_EQ_INT(2, l->token_list_buffer[1].token_length);

  ASSERT_EQ_INT(TOKEN_IDENT, l->token_list_buffer[2].token_type);
  ASSERT_EQ_INT(3, l->token_list_buffer[2].source_index);
  ASSERT_EQ_INT(1, l->token_list_buffer[2].token_length);

  ASSERT_EQ_INT(TOKEN_EOF, l->token_list_buffer[3].token_type);
  token_list_free(l);
}

/* [Part 4] "user_id2" is ONE TOKEN_IDENT spanning 8 chars. */
static void user_id2_is_ident_len8(void) {
  TokenList *l = tokenize_with("user_id2");
  ASSERT_NOT_NULL(l);
  ASSERT_EQ_INT(2, l->total_num_tkns);

  ASSERT_EQ_INT(TOKEN_IDENT, l->token_list_buffer[0].token_type);
  ASSERT_EQ_INT(0, l->token_list_buffer[0].source_index);
  ASSERT_EQ_INT(8, l->token_list_buffer[0].token_length);

  ASSERT_EQ_INT(TOKEN_EOF, l->token_list_buffer[1].token_type);
  token_list_free(l);
}

/* [Part 4] "users" still TOKEN_IDENT (len 5) — regression. */
static void users_still_ident_len5(void) {
  TokenList *l = tokenize_with("users");
  ASSERT_NOT_NULL(l);
  ASSERT_EQ_INT(2, l->total_num_tkns);

  ASSERT_EQ_INT(TOKEN_IDENT, l->token_list_buffer[0].token_type);
  ASSERT_EQ_INT(0, l->token_list_buffer[0].source_index);
  ASSERT_EQ_INT(5, l->token_list_buffer[0].token_length);

  ASSERT_EQ_INT(TOKEN_EOF, l->token_list_buffer[1].token_type);
  token_list_free(l);
}

/* [Part 4] "set" is still TOKEN_SET — keyword behavior unchanged. */
static void set_still_set(void) {
  TokenList *l = tokenize_with("set");
  ASSERT_NOT_NULL(l);
  ASSERT_EQ_INT(2, l->total_num_tkns);

  ASSERT_EQ_INT(TOKEN_SET, l->token_list_buffer[0].token_type);
  ASSERT_EQ_INT(0, l->token_list_buffer[0].source_index);
  ASSERT_EQ_INT(3, l->token_list_buffer[0].token_length);

  ASSERT_EQ_INT(TOKEN_EOF, l->token_list_buffer[1].token_type);
  token_list_free(l);
}

/* [Part 4] "seti" is ONE TOKEN_IDENT (len 4) — prefix guard survives. */
static void seti_still_ident(void) {
  TokenList *l = tokenize_with("seti");
  ASSERT_NOT_NULL(l);
  ASSERT_EQ_INT(2, l->total_num_tkns);

  ASSERT_EQ_INT(TOKEN_IDENT, l->token_list_buffer[0].token_type);
  ASSERT_EQ_INT(0, l->token_list_buffer[0].source_index);
  ASSERT_EQ_INT(4, l->token_list_buffer[0].token_length);

  ASSERT_EQ_INT(TOKEN_EOF, l->token_list_buffer[1].token_type);
  token_list_free(l);
}

/* [Part 5] "42" is TOKEN_NUMBER spanning 2 chars. */
static void number_42_is_number_then_eof(void) {
  TokenList *l = tokenize_with("42");
  ASSERT_NOT_NULL(l);
  ASSERT_EQ_INT(2, l->total_num_tkns);

  ASSERT_EQ_INT(TOKEN_NUMBER, l->token_list_buffer[0].token_type);
  ASSERT_EQ_INT(0, l->token_list_buffer[0].source_index);
  ASSERT_EQ_INT(2, l->token_list_buffer[0].token_length);

  ASSERT_EQ_INT(TOKEN_EOF, l->token_list_buffer[1].token_type);
  token_list_free(l);
}

/* [Part 5] "id=42;" -> IDENT, EQ, NUMBER, SEMI, EOF (5 tokens, all positions).
 */
static void id_eq_42_semi_is_ident_eq_number_semi_eof(void) {
  TokenList *l = tokenize_with("id=42;");
  ASSERT_NOT_NULL(l);
  ASSERT_EQ_INT(5, l->total_num_tkns);

  ASSERT_EQ_INT(TOKEN_IDENT, l->token_list_buffer[0].token_type);
  ASSERT_EQ_INT(0, l->token_list_buffer[0].source_index);
  ASSERT_EQ_INT(2, l->token_list_buffer[0].token_length);

  ASSERT_EQ_INT(TOKEN_EQ, l->token_list_buffer[1].token_type);
  ASSERT_EQ_INT(2, l->token_list_buffer[1].source_index);
  ASSERT_EQ_INT(1, l->token_list_buffer[1].token_length);

  ASSERT_EQ_INT(TOKEN_NUMBER, l->token_list_buffer[2].token_type);
  ASSERT_EQ_INT(3, l->token_list_buffer[2].source_index);
  ASSERT_EQ_INT(2, l->token_list_buffer[2].token_length);

  ASSERT_EQ_INT(TOKEN_SEMICOLON, l->token_list_buffer[3].token_type);
  ASSERT_EQ_INT(5, l->token_list_buffer[3].source_index);
  ASSERT_EQ_INT(1, l->token_list_buffer[3].token_length);

  ASSERT_EQ_INT(TOKEN_EOF, l->token_list_buffer[4].token_type);
  token_list_free(l);
}

/* [Part 6] "'hello'" -> one STRING whose span excludes both quotes. */
static void quoted_hello_is_string_with_quotes_excluded(void) {
  const char *input = "'hello'";
  TokenList *l = tokenize_with(input);
  ASSERT_NOT_NULL(l);
  ASSERT_EQ_INT(2, l->total_num_tkns);

  ASSERT_EQ_INT(TOKEN_STRING, l->token_list_buffer[0].token_type);
  ASSERT_EQ_INT(1, l->token_list_buffer[0].source_index);
  ASSERT_EQ_INT(5, l->token_list_buffer[0].token_length);
  ASSERT(l->token_list_buffer[0].token_string == input + 1);
  ASSERT_EQ_MEM("hello", l->token_list_buffer[0].token_string,
                l->token_list_buffer[0].token_length);

  ASSERT_EQ_INT(TOKEN_EOF, l->token_list_buffer[1].token_type);
  ASSERT_EQ_INT(7, l->token_list_buffer[1].source_index);
  token_list_free(l);
}

/* [Part 6] Interior whitespace is content, not a token separator. */
static void quoted_space_keeps_interior_whitespace(void) {
  const char *input = "'a b'";
  TokenList *l = tokenize_with(input);
  ASSERT_NOT_NULL(l);
  ASSERT_EQ_INT(2, l->total_num_tkns);

  ASSERT_EQ_INT(TOKEN_STRING, l->token_list_buffer[0].token_type);
  ASSERT_EQ_INT(1, l->token_list_buffer[0].source_index);
  ASSERT_EQ_INT(3, l->token_list_buffer[0].token_length);
  ASSERT_EQ_MEM("a b", l->token_list_buffer[0].token_string,
                l->token_list_buffer[0].token_length);

  ASSERT_EQ_INT(TOKEN_EOF, l->token_list_buffer[1].token_type);
  ASSERT_EQ_INT(5, l->token_list_buffer[1].source_index);
  token_list_free(l);
}

/* [Part 6] "''" is a zero-length STRING at position 1, not the end of input. */
static void empty_quoted_string_is_zero_length(void) {
  const char *input = "''";
  TokenList *l = tokenize_with(input);
  ASSERT_NOT_NULL(l);
  ASSERT_EQ_INT(2, l->total_num_tkns);

  ASSERT_EQ_INT(TOKEN_STRING, l->token_list_buffer[0].token_type);
  ASSERT_EQ_INT(1, l->token_list_buffer[0].source_index);
  ASSERT_EQ_INT(0, l->token_list_buffer[0].token_length);
  ASSERT(l->token_list_buffer[0].token_string == input + 1);

  ASSERT_EQ_INT(TOKEN_EOF, l->token_list_buffer[1].token_type);
  ASSERT_EQ_INT(2, l->token_list_buffer[1].source_index);
  token_list_free(l);
}

/* [Part 6] The quote ends at its closing quote; following bytes start fresh. */
static void quoted_string_does_not_swallow_following_ident(void) {
  TokenList *l = tokenize_with("'hi'x");
  ASSERT_NOT_NULL(l);
  ASSERT_EQ_INT(3, l->total_num_tkns);

  ASSERT_EQ_INT(TOKEN_STRING, l->token_list_buffer[0].token_type);
  ASSERT_EQ_INT(1, l->token_list_buffer[0].source_index);
  ASSERT_EQ_INT(2, l->token_list_buffer[0].token_length);
  ASSERT_EQ_MEM("hi", l->token_list_buffer[0].token_string,
                l->token_list_buffer[0].token_length);

  ASSERT_EQ_INT(TOKEN_IDENT, l->token_list_buffer[1].token_type);
  ASSERT_EQ_INT(4, l->token_list_buffer[1].source_index);
  ASSERT_EQ_INT(1, l->token_list_buffer[1].token_length);

  ASSERT_EQ_INT(TOKEN_EOF, l->token_list_buffer[2].token_type);
  ASSERT_EQ_INT(5, l->token_list_buffer[2].source_index);
  token_list_free(l);
}

/* [Part 6/7 boundary] The guide wants the OPENING quote's position on an
 * unterminated string. Contract decided 2026-11-26: tokenize returns the
 * partial list with had_error = true and error_pos at the offending byte;
 * the caller frees it normally. No NULL return on a lexical error. */
static void unterminated_string_reports_opening_quote(void) {
  TokenList *l = tokenize_with("'abc");
  ASSERT_NOT_NULL(l);
  ASSERT(l->had_error);
  ASSERT_EQ_INT(0, l->error_pos);
  ASSERT_EQ_INT(0, l->total_num_tkns);
  token_list_free(l);
}

/* [Part 7] A bad byte truncates the list: SELECT survives, EOF is not pushed.
 */
static void bad_byte_truncates_after_valid_token(void) {
  TokenList *l = tokenize_with("SELECT@");
  ASSERT_NOT_NULL(l);
  ASSERT(l->had_error);
  ASSERT_EQ_INT(6, l->error_pos);
  ASSERT_EQ_INT(1, l->total_num_tkns);
  ASSERT_EQ_INT(TOKEN_SELECT, l->token_list_buffer[0].token_type);
  token_list_free(l);
}

/* [Part 7] An error at byte 0 yields an empty list — there is no EOF token. */
static void bad_byte_at_start_gives_empty_list(void) {
  TokenList *l = tokenize_with("@SELECT");
  ASSERT_NOT_NULL(l);
  ASSERT(l->had_error);
  ASSERT_EQ_INT(0, l->error_pos);
  ASSERT_EQ_INT(0, l->total_num_tkns);
  token_list_free(l);
}

/* [Part 7] Bytes after the error are never examined. */
static void bad_byte_stops_scan_before_trailing_input(void) {
  TokenList *l = tokenize_with("SELECT 42 @ ;");
  ASSERT_NOT_NULL(l);
  ASSERT(l->had_error);
  ASSERT_EQ_INT(10, l->error_pos);
  ASSERT_EQ_INT(2, l->total_num_tkns);
  ASSERT_EQ_INT(TOKEN_SELECT, l->token_list_buffer[0].token_type);
  ASSERT_EQ_INT(TOKEN_NUMBER, l->token_list_buffer[1].token_type);
  token_list_free(l);
}

/* Part 8 — C defines free(NULL) as a no-op; token_list_free guards it too. */
static void token_list_free_null_is_safe(void) { token_list_free(NULL); }

/* Part 8 — fourteen realloc doublings, checked under ASan/UBSan. */
static void ten_thousand_tokens_stress(void) {
  const size_t token_count = 10000;
  const size_t input_len = token_count * 2;
  char *input = malloc(input_len + 1);
  ASSERT_NOT_NULL(input);

  for (size_t i = 0; i < token_count; i++) {
    input[2 * i] = 'a';
    input[2 * i + 1] = ' ';
  }
  input[input_len] = '\0';

  TokenList *l = tokenize(input);
  ASSERT_NOT_NULL(l);
  ASSERT(!l->had_error);
  ASSERT_EQ_INT(token_count + 1, l->total_num_tkns);

  for (size_t i = 0; i < token_count; i++) {
    ASSERT_EQ_INT(TOKEN_IDENT, l->token_list_buffer[i].token_type);
    ASSERT_EQ_INT(2 * i, l->token_list_buffer[i].source_index);
    ASSERT_EQ_INT(1, l->token_list_buffer[i].token_length);
  }

  const size_t eof = token_count;
  ASSERT_EQ_INT(TOKEN_EOF, l->token_list_buffer[eof].token_type);
  ASSERT_EQ_INT(input_len, l->token_list_buffer[eof].source_index);
  ASSERT_EQ_INT(0, l->token_list_buffer[eof].token_length);
  ASSERT(l->token_list_buffer[eof].token_string == input + input_len);

  token_list_free(l);
  free(input);
}

/* Part 9 — names come from the enum, with a fallback for bogus values. */
static void token_type_name_maps_known_and_unknown(void) {
  ASSERT_EQ_STR("SELECT", token_type_name(TOKEN_SELECT));
  ASSERT_EQ_STR("EOF", token_type_name(TOKEN_EOF));
  ASSERT_EQ_STR("UNKNOWN", token_type_name((tokentype)999));
}

/* Guide checklist — case-insensitive keywords beyond the FROM variant. */
static void select_case_variants_are_select(void) {
  const char *inputs[] = {"select", "SELECT", "SeLeCt"};
  for (size_t i = 0; i < sizeof(inputs) / sizeof(inputs[0]); i++) {
    TokenList *l = tokenize_with(inputs[i]);
    ASSERT_NOT_NULL(l);
    ASSERT_EQ_INT(2, l->total_num_tkns);
    ASSERT_EQ_INT(TOKEN_SELECT, l->token_list_buffer[0].token_type);
    ASSERT_EQ_INT(TOKEN_EOF, l->token_list_buffer[1].token_type);
    token_list_free(l);
  }
}

/* Guide "Done when" — every error case has a usable position, incl. lone `!`.
 */
static void lone_bang_reports_error(void) {
  TokenList *l = tokenize_with("a ! b");
  ASSERT_NOT_NULL(l);
  ASSERT(l->had_error);
  ASSERT_EQ_INT(2, l->error_pos);
  ASSERT_EQ_INT(1, l->total_num_tkns);
  ASSERT_EQ_INT(TOKEN_IDENT, l->token_list_buffer[0].token_type);
  token_list_free(l);
}

/* Part 10 — the chapter's one full statement, with exact spans. */
static void full_statement_ten_exact_tokens(void) {
  TokenList *l = tokenize_with("SELECT * FROM users WHERE id = 3;");
  ASSERT_NOT_NULL(l);
  ASSERT(!l->had_error);
  ASSERT_EQ_INT(10, l->total_num_tkns);

  static const tokentype types[] = {
      TOKEN_SELECT, TOKEN_STAR, TOKEN_FROM,   TOKEN_IDENT,     TOKEN_WHERE,
      TOKEN_IDENT,  TOKEN_EQ,   TOKEN_NUMBER, TOKEN_SEMICOLON, TOKEN_EOF,
  };
  static const size_t positions[] = {0, 7, 9, 14, 20, 26, 29, 31, 32, 33};
  static const size_t lengths[] = {6, 1, 4, 5, 5, 2, 1, 1, 1, 0};

  for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); i++) {
    ASSERT_EQ_INT(types[i], l->token_list_buffer[i].token_type);
    ASSERT_EQ_INT(positions[i], l->token_list_buffer[i].source_index);
    ASSERT_EQ_INT(lengths[i], l->token_list_buffer[i].token_length);
  }
  token_list_free(l);
}

/* Part 10 — spelling only; no grammar checks in the lexer. */
static void grammar_free_from_select_commas(void) {
  TokenList *l = tokenize_with("FROM SELECT , , ;");
  ASSERT_NOT_NULL(l);
  ASSERT(!l->had_error);
  ASSERT_EQ_INT(6, l->total_num_tkns);

  static const tokentype types[] = {TOKEN_FROM,  TOKEN_SELECT,    TOKEN_COMMA,
                                    TOKEN_COMMA, TOKEN_SEMICOLON, TOKEN_EOF};
  static const size_t positions[] = {0, 5, 12, 14, 16, 17};
  static const size_t lengths[] = {4, 6, 1, 1, 1, 0};

  for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); i++) {
    ASSERT_EQ_INT(types[i], l->token_list_buffer[i].token_type);
    ASSERT_EQ_INT(positions[i], l->token_list_buffer[i].source_index);
    ASSERT_EQ_INT(lengths[i], l->token_list_buffer[i].token_length);
  }
  token_list_free(l);
}

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

  /* Part 1 - punctuation */
  RUN_TEST(comma_is_comma_then_eof);
  RUN_TEST(semicolon_is_semicolon_then_eof);
  RUN_TEST(lparen_is_lparen_then_eof);
  RUN_TEST(rparen_is_rparen_then_eof);
  RUN_TEST(ident_comma_ident);

  /* Part 2 - single-char operators */
  RUN_TEST(eq_is_eq_then_eof);
  RUN_TEST(lt_is_lt_then_eof);
  RUN_TEST(gt_is_gt_then_eof);
  RUN_TEST(ident_gt_ident);

  /* Part 3 - two-char operators */
  RUN_TEST(lte_twochar);
  RUN_TEST(gte_twochar);
  RUN_TEST(neq_twochar);
  RUN_TEST(lt_single_still_lt);
  RUN_TEST(ident_lt_ident);
  RUN_TEST(ident_lte_ident);

  /* Part 5 - numbers */
  RUN_TEST(number_42_is_number_then_eof);
  RUN_TEST(id_eq_42_semi_is_ident_eq_number_semi_eof);

  /* Part 4 - identifier spelling */
  RUN_TEST(user_id2_is_ident_len8);
  RUN_TEST(users_still_ident_len5);
  RUN_TEST(set_still_set);
  RUN_TEST(seti_still_ident);

  /* Part 6 - quoted strings */
  RUN_TEST(quoted_hello_is_string_with_quotes_excluded);
  RUN_TEST(quoted_space_keeps_interior_whitespace);
  RUN_TEST(empty_quoted_string_is_zero_length);
  RUN_TEST(quoted_string_does_not_swallow_following_ident);
  RUN_TEST(unterminated_string_reports_opening_quote);

  /* Part 7 - lexical errors truncate the list; no EOF on the error path */
  RUN_TEST(bad_byte_truncates_after_valid_token);
  RUN_TEST(bad_byte_at_start_gives_empty_list);
  RUN_TEST(bad_byte_stops_scan_before_trailing_input);
  RUN_TEST(lone_bang_reports_error);

  /* Part 8 - list robustness */
  RUN_TEST(token_list_free_null_is_safe);
  RUN_TEST(ten_thousand_tokens_stress);

  /* Part 9 - token type names */
  RUN_TEST(token_type_name_maps_known_and_unknown);

  /* Guide checklist + Part 10 - end of chapter */
  RUN_TEST(select_case_variants_are_select);
  RUN_TEST(full_statement_ten_exact_tokens);
  RUN_TEST(grammar_free_from_select_commas);
}
