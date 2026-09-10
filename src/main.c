#include "repl.h"
#include <stdio.h>

int main(void) {
    repl_run(stdin, stdout);
    return 0;
}