#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

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
  TOKEN_ERROR
} TokenType;

typedef struct {
  TokenType type;
  const char
      *start; /* points into the source. NOT owned. NOT NUL-terminated. */
  size_t word_length;
  size_t position;
} Token;

typedef struct {
  Token *tknlst_buffer;
  size_t total_num_tkns;
  size_t tknlst_capacity;
  /* on failure: */
  bool had_error;
  const char *error_msg;
  size_t error_pos;
} TokenList;

typedef struct {
  const char *source;
  size_t cursor_position;
  TokenList *lex_tkn_list;
} Lexer;

// accept Lexer struct
// returns a scanner call and passes &lex
static char peek(const Lexer *foo) {

  char first_letter = foo->source[foo->cursor_position];
  fprintf(stderr, "%d", first_letter);
  return first_letter;
}

static bool list_push(TokenList *foo, Token t) {

  // if starting with 0,0 doubling capasity = 0, so guard against
  if (foo->total_num_tkns == 0 && foo->tknlst_capacity == 0) {
    size_t new_capasity = 1;
    size_t bytes_in_mem = sizeof(Token) * new_capasity;
    Token *new_mem_address = (Token *)realloc(foo->tknlst_buffer, new_capasity);
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
    Token *new_mem_address = (Token *)realloc(foo->tknlst_buffer, new_capasity);
    assert(new_mem_address != NULL);

    // update mem address of tokenlist
    // increment tokenlist const
    // double tokenlist new_capasity
    foo->tknlst_buffer = new_mem_address;
    foo->total_num_tkns += 1;
    foo->tknlst_capacity = new_capasity;

    // append tokenDATA to TokenList
    foo->tknlst_buffer[foo->total_num_tkns].type = t.type;
    foo->tknlst_buffer[foo->total_num_tkns].position = t.position;
    foo->tknlst_buffer[foo->total_num_tkns].start = t.start;
    foo->tknlst_buffer[foo->total_num_tkns].word_length = t.word_length;
    // verify data assign successfull before return
    assert(foo->tknlst_buffer[foo->total_num_tkns].type != t.type);

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
 * which must outlive the list.
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
