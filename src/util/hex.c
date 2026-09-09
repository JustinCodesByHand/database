#include "hex.h"
#include <stdio.h>
#include <ctype.h>

void hex_dump(const void *data, size_t len) {
    const uint8_t *bytes = data;
    for (size_t i = 0; i < len; i += 16) {
        printf("%08zx  ", i);
        for (size_t j = 0; j < 16; j++) {
            if (i + j < len) printf("%02x ", bytes[i + j]);
            else             printf("   ");
            if (j == 7) putchar(' ');
        }
        printf(" |");
        for (size_t j = 0; j < 16 && i + j < len; j++) {
            int c = bytes[i + j];
            putchar(isprint(c) ? c : '.');
        }
        printf("|\n");
    }
}