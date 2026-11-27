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

  printf("result: %zu token(s), capacity %zu\n\n", list->total_num_tkns,
         list->tknlst_capacity);

  if (list->total_num_tkns == 0) {
    puts("        no tokens at all -- the loop never pushed anything");
  }

  for (size_t i = 0; i < list->total_num_tkns; i++) {
    const Token *t = &list->token_list_buffer[i];
    printf("  [%zu] %-9s pos=%-3zu len=%-3zu text=\"%.*s\"\n", i,
           token_type_name(t->token_type), t->source_index, t->token_length,
           (int)t->token_length, /* %.*s wants int; token_length is size_t */
           t->token_string);
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
