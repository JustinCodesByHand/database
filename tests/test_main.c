#include "test.h"

int tests_run = 0;
int tests_failed = 0;
const char *current_test = "";

/* one declaration per test file */
void suite_repl(void);

int main(void) {
    suite_repl();

    printf("\n%d tests, %d failed\n", tests_run, tests_failed);
    return tests_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
