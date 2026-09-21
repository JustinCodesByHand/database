#define _POSIX_C_SOURCE 200809L
#include "test.h"
#include "repl.h"

#include <stdlib.h>
#include <string.h>

/*
 * Runs the repl over `input` and returns everything it printed.
 * CALLER OWNS the returned string — free() it.
 */
static char *run_with(const char *input) {
    FILE *in = fmemopen((void *)input, strlen(input), "r");

    char  *out_buf = NULL;
    size_t out_len = 0;
    FILE  *lex_tkn_list = open_memstream(&out_buf, &out_len);

    repl_run(in, lex_tkn_list);

    fclose(lex_tkn_list);            /* MUST close before out_buf is valid */
    fclose(in);
    return out_buf;         /* caller frees */
}

static void prints_prompt_before_reading(void) {
    char *foo = run_with(".exit\n");
    ASSERT_STR_CONTAINS(foo, "minidb> ");
    free(foo);
}

static void exits_on_exit_command(void) {
    /* if .exit didn't stop the loop, the second line would also be processed */
    char *foo = run_with(".exit\nnonsense\n");
    ASSERT(strstr(foo, "nonsense") == NULL);
    free(foo);
}

static void exits_cleanly_at_end_of_input(void) {
    /* no .exit at all — getline returns -1. Must not hang or crash. */
    char *foo = run_with("");
    ASSERT_EQ_STR("minidb> ", foo);
    free(foo);
}

void suite_repl(void) {
    SUITE("repl");
    RUN_TEST(prints_prompt_before_reading);
    RUN_TEST(exits_on_exit_command);
    RUN_TEST(exits_cleanly_at_end_of_input);
}