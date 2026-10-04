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
// factor out the conditional logic for instanciation, size extention, else
static void grow_list_capisity(TokenList *tkn_list_struct) {
  // guard against 0,0 case when doubling memory size
  //
  size_t new_capasity;

  if (tkn_list_struct->tknlst_capacity == 0) {
    new_capasity = 1;
  } else {
    new_capasity = tkn_list_struct->tknlst_capacity * 2;
  }

  size_t new_mem_size = sizeof(Token) * new_capasity;
  Token *new_mem_address =
      (Token *)realloc(tkn_list_struct->token_list_buffer, new_mem_size);
  if (new_mem_address != NULL) {
    tkn_list_struct->token_list_buffer = new_mem_address;
    tkn_list_struct->tknlst_capacity = new_capasity;
  } else {
    tkn_list_struct->had_error = true;
  }
}

// Helper for list_push
// assigns new token valies to next index in the tkn_list_struct
static void add_tkn_to_tknlist(TokenList *tkn_list_struct, Token newToken) {
  tkn_list_struct->total_num_tkns += 1;
  // append tokenDATA to TokenList
  tkn_list_struct->token_list_buffer[tkn_list_struct->total_num_tkns - 1]
      .token_type = newToken.token_type;
  tkn_list_struct->token_list_buffer[tkn_list_struct->total_num_tkns - 1]
      .start_index = newToken.start_index;
  tkn_list_struct->token_list_buffer[tkn_list_struct->total_num_tkns - 1]
      .input_string = newToken.input_string;
  tkn_list_struct->token_list_buffer[tkn_list_struct->total_num_tkns - 1]
      .word_length = newToken.word_length;
}

// lexer Helper that adds new tokens to its list
// realloc memory if list grows to big
//  transfers data from new token to tokenlist
static bool list_push(TokenList *tkn_list_struct, Token newToken) {

  if (tkn_list_struct->tknlst_capacity == tkn_list_struct->total_num_tkns) {
    grow_list_capisity(tkn_list_struct);
  }

  if (tkn_list_struct->had_error == true) {
    tkn_list_struct->had_error = false; // resetting
    // TODO: assign error_msg paramaters
    return false;
  } else {
    add_tkn_to_tknlist(tkn_list_struct, newToken);
    return true;
  }
}

// Tokenlist Helper
// free allocated he memory after finished using
void token_list_free(TokenList *tkn_list_struct) {
  if (tkn_list_struct != NULL) {
    free(tkn_list_struct->token_list_buffer);
    free(tkn_list_struct);
  }
}

// Lexer Helper
// increrment cursor position, so peek() sees next character
static void increment_cursor(Lexer *lexer) { lexer->cursor_position += 1; }

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
  lexers_tkn_list->token_list_buffer = NULL;
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
  lexer.lex_tkn_list_struct = lexers_tkn_list;

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
    bool add_to_list = list_push(lexer.lex_tkn_list_struct, eof_tkn);

    if (!add_to_list) {
      token_list_free(lexer.lex_tkn_list_struct);
      // TODO:return error message
      return NULL;
    }
    // exits loop if append eof token
  }

  return lexers_tkn_list;
}
