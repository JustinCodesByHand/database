#include "tokenizer.h"
#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

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

// init OR grow the list capasity
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
static void addToStruct(TokenList *tkn_list_struct, Token newToken) {
  // append tokenDATA to TokenList
  tkn_list_struct->token_list_buffer[tkn_list_struct->total_num_tkns - 1]
      .token_type = newToken.token_type;
  tkn_list_struct->token_list_buffer[tkn_list_struct->total_num_tkns - 1]
      .offset_position = newToken.offset_position;
  tkn_list_struct->token_list_buffer[tkn_list_struct->total_num_tkns - 1]
      .input_string = newToken.input_string;
  tkn_list_struct->token_list_buffer[tkn_list_struct->total_num_tkns - 1]
      .word_length = newToken.word_length;
}

// lexer Helper that adds new tokens to its list
// realloc memory if list grows to bigggg
//  transfers data from new token to tokenlist
static bool tkn_list_add(TokenList *tkn_list_struct, Token newToken) {

  if (tkn_list_struct->tknlst_capacity == tkn_list_struct->total_num_tkns) {
    grow_list_capisity(tkn_list_struct);
  }

  if (tkn_list_struct->had_error != true) {
    // TODO: assign error_msg paramaters
    tkn_list_struct->total_num_tkns += 1;
    addToStruct(tkn_list_struct, newToken);
    return true;
  } else {
    newToken.token_type = token_error;
    tkn_list_struct->had_error = true;
    tkn_list_struct->error_pos = newToken.offset_position;
    tkn_list_struct->error_msg =
        "grow_list_capisity threw an error, it uses malloc and realloc";
    return false;
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

typedef struct {
  const char *word;
  tokentype keywordType;
} keyword;

static const keyword KEYWORDS[] = {
    {"select", TOKEN_SELECT}, {"from", TOKEN_FROM},   {"where", TOKEN_WHERE},
    {"insert", TOKEN_INSERT}, {"into", TOKEN_INTO},   {"values", TOKEN_VALUES},
    {"create", TOKEN_CREATE}, {"table", TOKEN_TABLE}, {"delete", TOKEN_DELETE},
    {"update", TOKEN_UPDATE}, {"set", TOKEN_SET}};

// Lexer Helper:
// creates and returns a new token that starts isalpha()
static Token tokenize_word(Lexer *lexer, size_t starting_index) {
  Token new_word_Token;
  new_word_Token.offset_position = starting_index;
  new_word_Token.word_length =
      lexer->cursor_position - new_word_Token.offset_position;
  new_word_Token.input_string = lexer->input_string + starting_index;
  new_word_Token.token_type = TOKEN_IDENT; // default value

  for (size_t i = 0; i < (sizeof(KEYWORDS) / sizeof(KEYWORDS[0])); i++) {
    if (new_word_Token.word_length == strlen(KEYWORDS[i].word) &&
        strncasecmp(new_word_Token.input_string, KEYWORDS[i].word,
                    new_word_Token.word_length) == 0) {
      new_word_Token.token_type = KEYWORDS[i].keywordType;
    }
  }

  return new_word_Token;
}

typedef struct {
  const char *symbol;
  tokentype punct_type;
} punctuation;

static const punctuation PUNCTUATIONS[] = {
    {"*", TOKEN_STAR},      {":", TOKEN_SEMICOLON}, {",", TOKEN_COMMA},
    {";", TOKEN_SEMICOLON}, {"(", TOKEN_LPAREN},    {")", TOKEN_RPAREN},
    {"=", TOKEN_EQ},        {"!=", TOKEN_NEQ},      {"<", TOKEN_LT},
    {"<=", TOKEN_LTE},      {">", TOKEN_GT},        {">=", TOKEN_GTE}};

// us same pattern as tokenize word
static Token tokenize_punctuation(Lexer *lexer, size_t starting_index) {

  Token new_punctuation_token = (Token){
      TOKEN_IDENT, lexer->input_string + starting_index, 1, starting_index};

  for (size_t i = 0; i < (sizeof(PUNCTUATIONS) / sizeof(PUNCTUATIONS)[0]);
       i++) {
    if ((strncasecmp(new_punctuation_token.input_string, PUNCTUATIONS[i].symbol,
                     new_punctuation_token.word_length)) == 0) {
      new_punctuation_token.token_type = PUNCTUATIONS[i].punct_type;
      return new_punctuation_token;
    }
  }
  // lexer->lex_tkn_list_struct->had_error = true;
  return new_punctuation_token;
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

  // loopty loop time ie. main loop
  // looks at each char and adds a token to the higher level token list struct
  // which is inside the higher level token_list_buffer

  // while we have not encountered '\0'
  while (!is_at_end(&lexer)) {

    // consume whitespaces and increment the current cursor position
    while (isspace((unsigned char)peek(&lexer))) {
      lexer.cursor_position += 1;
    }

    // trailinr whitespace: nothing is left but the EOF marker
    // r
    if (is_at_end(&lexer)) {
      break;
    }

    // everything that is not whitespace starts a token here
    size_t starting_offset_position = lexer.cursor_position;

    // querey the char to see if its a letter
    if (isalpha((unsigned char)peek(&lexer))) {

      // continue reading word while scanning contunious letters
      while (isalpha((unsigned char)peek(&lexer))) {
        // increment the count until we encounter a non-letter
        lexer.cursor_position += 1;
      }

      // assign feilds after reading entire word
      Token word_tkn = tokenize_word(&lexer, starting_offset_position);
      tkn_list_add(lexer.lex_tkn_list_struct, word_tkn);

    } else {
      Token punctuation_token;
      bool made_punct = false;
      switch (peek(&lexer)) {
      case '*':
        punctuation_token =
            tokenize_punctuation(&lexer, starting_offset_position);
        lexer.cursor_position += 1;
        made_punct = true;
        break;

      case ';':
        punctuation_token =
            tokenize_punctuation(&lexer, starting_offset_position);
        lexer.cursor_position += 1;
        made_punct = true;
        break;

      default:
        lexer.cursor_position += 1;
        break;
      };
      // Unrecognised character (operator, punctuation, digit, ...). We do
      // not tokenise these yet, but we MUST consume one so the loop can
      // never spin forever on the same char.
      // TODO: emit real tokens (e.g. ';' -> TOKEN_SEMICOLON) and/or error
      // reporting (error_msg + error_pos + had_error) — see the open design
      // question about how OOM/lex errors should surface.
      if (made_punct) {
        tkn_list_add(lexer.lex_tkn_list_struct, punctuation_token);
      }
    }
  }
  // if we read '\0', then make EOF token and exit (we always are at end here)
  Token eof_tkn = {TOKEN_EOF, lexer.input_string, 0, lexer.cursor_position};
  bool add_to_list = tkn_list_add(lexer.lex_tkn_list_struct, eof_tkn);

  if (!add_to_list && (lexer.lex_tkn_list_struct->had_error == true)) {
    lexer.lex_tkn_list_struct->had_error = false;
    fprintf(stderr, "%s", "errorrrr");
    return NULL;
  }

  return lexers_tkn_list;
}
