#include "tokenizer.h"
#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

// aaaaccept Lexer struct some changesaaaaaaaaaaaaa:
// returns a scanner call and passes &lex
static char peek(const Lexer *foo) {

  char first_letter = foo->source[foo->cursor_position];
  return first_letter;
}

void token_list_free(TokenList *foo) {
  if (foo != NULL) {
    free(foo->tknlst_buffer);
    free(foo);
  } else {
    fprintf(stderr, "%s", "cant free");
  }
}

static bool list_push(TokenList *foo, Token t) {
  // if starting with 0,0 doubling capasity = 0, so guard against
  if (foo->total_num_tkns == 0 && foo->tknlst_capacity == 0) {
    size_t new_capasity = 1;
    size_t bytes_in_mem = sizeof(Token) * new_capasity;
    Token *new_mem_address = (Token *)realloc(foo->tknlst_buffer, bytes_in_mem);
    assert(new_mem_address != NULL);

    foo->tknlst_buffer = new_mem_address;
    foo->total_num_tkns += 1;
    foo->tknlst_capacity = new_capasity;

    // append tokenDATA to TokenList
    foo->tknlst_buffer[foo->total_num_tkns - 1].type = t.type;
    foo->tknlst_buffer[foo->total_num_tkns - 1].position = t.position;
    foo->tknlst_buffer[foo->total_num_tkns - 1].start = t.start;
    foo->tknlst_buffer[foo->total_num_tkns - 1].word_length = t.word_length;
    // verify data assign successfull before return
    return true;
  }

  // if at capasity  expand + reassign mem address
  if (foo->total_num_tkns == foo->tknlst_capacity) {

    // calulate size of new mem block
    size_t new_capasity;
    new_capasity = foo->tknlst_capacity * 2;
    size_t bytes_in_mem = sizeof(Token) * new_capasity;

    // captures the new address from realloc
    Token *new_mem_address = (Token *)realloc(foo->tknlst_buffer, bytes_in_mem);
    assert(new_mem_address != NULL);

    // update mem address of tokenlist
    // increment tokenlist const
    // double tokenlist new_capasity
    foo->tknlst_buffer = new_mem_address;
    foo->total_num_tkns += 1;
    foo->tknlst_capacity = new_capasity;

    // append tokenDATA to TokenList
    foo->tknlst_buffer[foo->total_num_tkns - 1].type = t.type;
    foo->tknlst_buffer[foo->total_num_tkns - 1].position = t.position;
    foo->tknlst_buffer[foo->total_num_tkns - 1].start = t.start;
    foo->tknlst_buffer[foo->total_num_tkns - 1].word_length = t.word_length;

    return true;
  }
  return false;
}

static bool is_at_end(const Lexer *pbR_lexer) {
  // peek at first char and see if \0
  char nul_term = peek(pbR_lexer);
  if (nul_term == '\0') {
    return true;
  } else {
    return false;
  }
}

/*
 * Tokenizes `source`. The returned list points INTO `source`,
 * which must outlive the list. aaaaa
 * CALLER OWNS the result — call token_list_free().
 */
TokenList *tokenize(const char *source) {
  // list  remains persistant btw calls
  TokenList *lexers_tkn_list = malloc(sizeof(TokenList));
  lexers_tkn_list->tknlst_buffer = NULL;
  lexers_tkn_list->total_num_tkns = 0;
  lexers_tkn_list->tknlst_capacity = 0;
  lexers_tkn_list->had_error = false;
  lexers_tkn_list->error_msg = "no error\n";
  lexers_tkn_list->error_pos = 0;

  Lexer lex;
  lex.source = source;
  lex.cursor_position = 0;
  lex.lex_tkn_list = lexers_tkn_list;
  assert(lex.lex_tkn_list != NULL);

  // dispatch loop

  bool finished_processing = false;

  while (finished_processing != true) {
    // TODO: add logic to skipp whitespaces.
    if (is_at_end(&lex) == true) {
      Token is_at_end = {TOKEN_EOF, lex.source, 0, lex.cursor_position};
      bool list_append_success = list_push(lex.lex_tkn_list, is_at_end);
      finished_processing = list_append_success;
      // exits loop if append eof token
    }
  }

  return lexers_tkn_list;
}
