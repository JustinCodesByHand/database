#ifndef MINIDB_AST_H
#define MINIDB_AST_H

#include <stddef.h>
#include <stdint.h>

/* Lifetime chain: the source string outlives the TokenList outlives the AST
 * arena. Every string in this file is a (pointer, length) view into the source:
 * nobody owns it, nobody copies it, and it is not NUL-terminated at its end.
 * The caller owns the arena; no individual node is ever freed. */

typedef enum {
  EXPR_LITERAL_INT,
  EXPR_LITERAL_STR,
  EXPR_COLUMN,
  EXPR_BINARY
} ExprType;
typedef enum { OP_EQ, OP_NEQ, OP_LT, OP_LTE, OP_GT, OP_GTE } BinaryOp;

typedef struct Expr Expr;
struct Expr {
  ExprType type;
  union {
    struct {
      int64_t value;
    } literal_int;
    struct {
      const char *start;
      size_t length;
    } literal_str;
    struct {
      const char *start;
      size_t length;
    } column;
    struct {
      Expr *left;
      BinaryOp op;
      Expr *right;
    } binary;
  } as;
};

typedef enum { STMT_SELECT, STMT_INSERT } StatementType;

typedef struct {
  StatementType type;
  union {
    struct {
      const char *table;
      size_t table_len;
      /* column_count == 0 means SELECT * */
      const char **columns;
      size_t *column_lens;
      size_t column_count;
      Expr *where; /* NULL if no WHERE clause */
    } select;
    struct {
      const char *table;
      size_t table_len;
      Expr **values;
      size_t value_count;
    } insert;
  } as;
} Statement;

#endif
