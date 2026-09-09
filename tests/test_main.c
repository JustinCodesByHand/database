#include "test.h"

int tests_run = 0;
int tests_failed = 0;
const char *current_test = "";

/* one declaration per test file */
void suite_repl(void);
void suite_tokenizer(void);
void suite_row(void);
void suite_pager(void);
void suite_btree(void);

int main(void) {
    suite_repl();
    suite_tokenizer();
    suite_row();
    suite_pager();
    suite_btree();

    printf("\n%d tests, %d failed\n", tests_run, tests_failed);
    return tests_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
