#include "tokenizer.h"
#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

// looks at the character in int(current_position)
// usage: returns singular character at cursor_position
static unsigned char peek(const Lexer *lexer) {
  unsigned char first_letter =
      (unsigned char)lexer->input_string[lexer->cursor_position];
  return first_letter;
}

// BOOL - check for terminating character '\0'
// usage: peek at each character looking for nul_term
static bool is_at_end(const Lexer *lexer) {
  // peek at first char and see if \0
  unsigned char nul_term = peek(lexer);
  if (nul_term == '\0') {
    return true;
  } else {
    // program continues
    return false;
  }
}

// TODO: instanciate OR grow the list capasity
//
static void grow_list_capisity() {}

static void add_tkn_to_tknlist(TokenList *tkn_list_struct, Token newToken) {
  tkn_list_struct->total_num_tkns += 1;
  // append tokenDATA to TokenList
  tkn_list_struct->token_list[tkn_list_struct->total_num_tkns - 1].token_type =
      newToken.token_type;
  tkn_list_struct->token_list[tkn_list_struct->total_num_tkns - 1].start_index =
      newToken.start_index;
  tkn_list_struct->token_list[tkn_list_struct->total_num_tkns - 1]
      .input_string = newToken.input_string;
  tkn_list_struct->token_list[tkn_list_struct->total_num_tkns - 1].word_length =
      newToken.word_length;
}

// lexer Helper that adds new tokens to its list
// realloc memory if list grows to big
//  transfers data from new token to tokenlist
static bool list_push(TokenList *tkn_list_struct, Token newToken) {
  // if starting with 0,0 doubling capasity = 0, so guard against
  // Problem 2 * 0 = 0, cant double 0
  if (tkn_list_struct->total_num_tkns == 0 &&
      tkn_list_struct->tknlst_capacity == 0) {
    size_t new_capasity = 1;
    size_t bytes_in_mem = sizeof(Token) * new_capasity;
    Token *new_mem_address =
        (Token *)realloc(tkn_list_struct->token_list, bytes_in_mem);
    assert(new_mem_address != NULL);

    tkn_list_struct->token_list = new_mem_address;
    tkn_list_struct->tknlst_capacity = new_capasity;

    add_tkn_to_tknlist(tkn_list_struct, newToken);
    return true;
  }

  // if at capasity  expand + reassign mem address
  if (tkn_list_struct->total_num_tkns == tkn_list_struct->tknlst_capacity) {

    // calulate size of new mem block
    size_t new_capasity;
    new_capasity = tkn_list_struct->tknlst_capacity * 2;
    size_t bytes_in_mem = sizeof(Token) * new_capasity;

    // captures the new address from realloc
    Token *new_mem_address =
        (Token *)realloc(tkn_list_struct->token_list, bytes_in_mem);
    // Realloc auto frees previous mem address
    if (new_mem_address == NULL) {
      new_mem_address = tkn_list_struct->token_list;
      fprintf(stderr, "%s", "realloc did not properly allocate");
    }

    // update mem address of tokenlist
    // increment tokenlist const
    // double tokenlist new_capasity
    // TODO: make a function
    tkn_list_struct->token_list = new_mem_address;
    tkn_list_struct->tknlst_capacity = new_capasity;

    add_tkn_to_tknlist(tkn_list_struct, newToken);
    return true;
  }
  // default: adds token to the list because there is space available
  if (tkn_list_struct->total_num_tkns < tkn_list_struct->tknlst_capacity) {
    // assign the new token to the list at index +1
    add_tkn_to_tknlist(tkn_list_struct, newToken);
    return true;
  }
  // FIX:populate and return the error messages of Lex
  return false;
}

// Tokenlist Helper
// free allocated he memory after finished using
void token_list_free(TokenList *tkn_list_struct) {
  if (tkn_list_struct != NULL) {
    free(tkn_list_struct->token_list);
    free(tkn_list_struct);
  }
}

// Lexer Helper
// increrment cursor position, so peek() sees next character
void increment_cursor(Lexer *lexer) {
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
  assert(lexers_tkn_list != NULL);
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

  // loopty loop time
  // looks at each char and does something idk

  // while we have not encountered '\0'
  while (!is_at_end(&lexer)) {

    // consume whitespaces and increment the current cursor position
    while (isspace((unsigned char)peek(&lexer))) {
      increment_cursor(&lexer);
    }

    // TODO: index into the string using
    // tokens's word_length, start_index,*input_string
    while (isalpha((unsigned char)peek(&lexer)) | peek(&lexer)) {
      // increment the count until we encounter a space
    }
  }
  // if we read '\0', then make EOF token and exit
  if (is_at_end(&lexer) == true) {
    Token eof_tkn = {TOKEN_EOF, lexer.input_string, 0, lexer.cursor_position};
    bool add_to_list = list_push(lexer.lex_tkn_list, eof_tkn);

    if (!add_to_list) {
      free(lexer.lex_tkn_list);
      // TODO:return error message
      return NULL;
    }
    // exits loop if append eof token
  }

  return lexers_tkn_list;
}
