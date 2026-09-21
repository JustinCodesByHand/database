#ifndef MINIDB_REPL_H
#define MINIDB_REPL_H

#include <stdio.h>

/*
 * Reads lines from `in`, writes results to `lex_tkn_list`, until .exit or EOF.
 * Does NOT take ownership of either stream — the caller closes them.
 */
void repl_run(FILE *in, FILE *lex_tkn_list);

#endif
