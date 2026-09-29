#include "tokenizer.h"
#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

// looks at the character in int(current_position)
// usage: returns singular character at cursor_position
static unsigned char peek(const Lexer *foo) {
  unsigned char first_letter =
      (unsigned char)foo->input_string[foo->cursor_position];
  return first_letter;
}

// BOOL - check for terminating character '\0'
// usage: peek at each character looking for nul_term
static bool is_at_end(const Lexer *pbR_lexer) {
  // peek at first char and see if \0
  unsigned char nul_term = peek(pbR_lexer);
  if (nul_term == '\0') {
    return true;
  } else {
    // program continues
    return false;
  }
}

// lexer Helper that adds new tokens to its list
// realloc memory if list grows to big
//  transfers data from new token to tokenlist
static bool list_push(TokenList *foo, Token newToken) {
  // if starting with 0,0 doubling capasity = 0, so guard against
  // Problem 2 * 0 = 0, cant double 0
  if (foo->total_num_tkns == 0 && foo->tknlst_capacity == 0) {
    size_t new_capasity = 1;
    size_t bytes_in_mem = sizeof(Token) * new_capasity;
    Token *new_mem_address = (Token *)realloc(foo->token_list, bytes_in_mem);
    assert(new_mem_address != NULL);

    // TODO: make a function
    foo->token_list = new_mem_address;
    foo->total_num_tkns += 1;
    foo->tknlst_capacity = new_capasity;

    // append tokenDATA to TokenList
    foo->token_list[foo->total_num_tkns - 1].token_type = newToken.token_type;
    foo->token_list[foo->total_num_tkns - 1].position = newToken.position;
    foo->token_list[foo->total_num_tkns - 1].input_string =
        newToken.input_string;
    foo->token_list[foo->total_num_tkns - 1].word_length = newToken.word_length;
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
    Token *new_mem_address = (Token *)realloc(foo->token_list, bytes_in_mem);
    assert(new_mem_address != NULL);

    // update mem address of tokenlist
    // increment tokenlist const
    // double tokenlist new_capasity
    // TODO: make a function
    foo->token_list = new_mem_address;
    foo->total_num_tkns += 1;
    foo->tknlst_capacity = new_capasity;

    // append tokenDATA to TokenList
    foo->token_list[foo->total_num_tkns - 1].token_type = newToken.token_type;
    foo->token_list[foo->total_num_tkns - 1].position = newToken.position;
    foo->token_list[foo->total_num_tkns - 1].input_string =
        newToken.input_string;
    foo->token_list[foo->total_num_tkns - 1].word_length = newToken.word_length;

    return true;
  }
  // TODO:populate and return the error messages of Lex
  return false;
}

// Tokenlist Helper
// free allocated he memory after finished using
void token_list_free(TokenList *foo) {
  if (foo != NULL) {
    free(foo->token_list);
    free(foo);
  } else {
    fprintf(stderr, "%s", "cant free  a NUll list");
  }
}

// Lexer Helper
// increrment cursor position, so peek() sees next character
void move_cursor(Lexer *lexer) {
  lexer->cursor_position += 1;
  // TODO: decide to add guard against over-read
}

/*
 * Tokenizes `source`. The returned list points INTO `source`,
 * which must outlive the list. aaaaa
 * CALLER OWNS the result — call token_list_free().
 */
TokenList *tokenize(const char *input_string) {
  // instanciating my top level data structure
  // they live outlive subsiquent function calls
  TokenList *lexers_tkn_list = malloc(sizeof(TokenList)); // must free()
  lexers_tkn_list->token_list = NULL;
  lexers_tkn_list->total_num_tkns = 0;
  lexers_tkn_list->tknlst_capacity = 0;
  // internal error logging
  lexers_tkn_list->had_error = false;
  lexers_tkn_list->error_msg = "no error\n";
  lexers_tkn_list->error_pos = 0;

  // our top level object folks
  // point our malloc'd(Tknlst) to the lexer's
  Lexer lexer;
  lexer.input_string = input_string;
  lexer.cursor_position = 0;
  lexer.lex_tkn_list = lexers_tkn_list;
  assert(lexer.lex_tkn_list != NULL);

  // loopty loop time
  // looks at each char and does something idk

  // while we have not encountered '\0'
  while (!is_at_end(&lexer)) {
    //
    // FIX: add logic to skipp whitespaces.
    while (isspace((unsigned char)peek(&lexer))) {
      move_cursor(&lexer);
      // guarding against reading past EOF '\0'
      if (is_at_end(&lexer)) {
        break; // the top level loop catches the EOF '\0'
      }
    }
  }
  // if we read '\0', then make EOF token and exit
  if (is_at_end(&lexer) == true) {
    Token is_at_end = {TOKEN_EOF, lexer.input_string, 0, lexer.cursor_position};
    bool add_to_list = list_push(lexer.lex_tkn_list, is_at_end);
    //
    // TODO: add appropiate error messages
    if (!add_to_list) {
      return NULL;
    }
    // exits loop if append eof token
  }

  return lexers_tkn_list;
}
