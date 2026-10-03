/* SCRATCH HARNESS -- a debugger you can point at any string.
 *
 * NOT part of the real test suite. It has its own main(), so the Makefile
 * filters it out of TEST_SRC (otherwise: "multiple definition of `main'").
 *
 *   make scratch                # uses the DEFAULT_INPUT below
 *   make scratch ARG='"SELECT"'
 *   make scratch ARG='users'
 *
 * It is your eyes while the lexer is broken. The first thing to do when
 * something looks wrong is print the token list and see what actually came out.
 */
#include "tokenizer.h"

#include <stdio.h>
#include <string.h>

#define DEFAULT_INPUT "SELECT"

/* TEMPORARY. token_type_name() is declared in tokenizer.h but not yet defined
 * anywhere (fix #6 on the list), so this file cannot call it yet. Delete this
 * function the moment you implement token_type_name() and call that instead --
 * it is the same job, and the real one has no default: on purpose, so a new
 * TokenType becomes a build error instead of a silent "?". */
static const char *scratch_type_name(TokenType type) {
  switch (type) {
    case TOKEN_SELECT:  return "SELECT";
    case TOKEN_FROM:    return "FROM";
    case TOKEN_WHERE:   return "WHERE";
    case TOKEN_INSERT:  return "INSERT";
    case TOKEN_INTO:    return "INTO";
    case TOKEN_VALUES:  return "VALUES";
    case TOKEN_CREATE:  return "CREATE";
    case TOKEN_TABLE:   return "TABLE";
    case TOKEN_DELETE:  return "DELETE";
    case TOKEN_UPDATE:  return "UPDATE";
    case TOKEN_SET:     return "SET";
    case TOKEN_STAR:    return "STAR";
    case TOKEN_COMMA:   return "COMMA";
    case TOKEN_SEMICOLON: return "SEMICOLON";
    case TOKEN_LPAREN:  return "LPAREN";
    case TOKEN_RPAREN:  return "RPAREN";
    case TOKEN_EQ:      return "EQ";
    case TOKEN_NEQ:     return "NEQ";
    case TOKEN_LT:      return "LT";
    case TOKEN_LTE:     return "LTE";
    case TOKEN_GT:      return "GT";
    case TOKEN_GTE:     return "GTE";
    case TOKEN_NUMBER:  return "NUMBER";
    case TOKEN_STRING:  return "STRING";
    case TOKEN_IDENT:   return "IDENT";
    case TOKEN_EOF:     return "EOF";
    case TOKEN_ERROR:   return "ERROR";
    default:            return "UNKNOWN";
  }
}

int main(int argc, char **argv) {
  const char *input = (argc > 1) ? argv[1] : DEFAULT_INPUT;

  printf("input : \"%s\"  (%zu bytes)\n", input, strlen(input));
  fflush(stdout);

  /* A real hang shows up as this line printing and nothing after it.
   * Run it with:  timeout 5 ./build/scratch ; echo "exit=$?"  -> 124 = hung. */
  TokenList *list = tokenize(input);

  if (list == NULL) {
    puts("result: tokenize() returned NULL");
    puts("        (either an OOM path, or list_push ran out of room)");
    return 1;
  }

  printf("result: %zu token(s), capacity %zu\n\n",
         list->total_num_tkns, list->tknlst_capacity);

  if (list->total_num_tkns == 0) {
    puts("        no tokens at all -- the loop never pushed anything");
  }

  for (size_t i = 0; i < list->total_num_tkns; i++) {
    const Token *t = &list->token_list[i];
    printf("  [%zu] %-9s pos=%-3zu len=%-3zu text=\"%.*s\"\n",
           i,
           scratch_type_name(t->token_type),
           t->position,
           t->word_length,
           (int)t->word_length,   /* %.*s wants int; word_length is size_t */
           t->input_string);
  }

  if (list->had_error) {
    printf("\n  had_error = true   error_pos = %zu   error_msg = %s",
           list->error_pos, list->error_msg);
  } else {
    puts("\n  had_error = false");
  }

  token_list_free(list);
  return 0;
}
