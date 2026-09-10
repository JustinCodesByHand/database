#define _POSIX_C_SOURCE 200809L
#include "repl.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define PROMPT "minidb> "

typedef enum { OUTCOME_CONTINUE, OUTCOME_EXIT } Outcome;

/* Handles one non-blank, trimmed line. */
static Outcome handle_line(const char *line, FILE *out) {
    /* TODO: leading '.' -> meta-command
     *       otherwise    -> we can't parse SQL yet; say so
     */
    if (line[0] == '.')
    {
        
    }
    else
    {
        printf("Cant parse sql.\n");
    }
    
    
    (void)line; (void)out;
    return OUTCOME_CONTINUE;
}

/* Removes a trailing '\n' (and '\r' if present) in place. */
static void strip_newline(char *line) {
    /* TODO */

    size_t string_end_index = strcspn(line, "\n\r");
    line[string_end_index] = '\0';
}

/* Returns a pointer to the first non-space char; trims trailing space in place. */
static char *trim(char *line) {
    /* TODO */
    int line_length = (int)strlen(line);
    char *new_start_ptr = line + line_length;

    //move new start ptr past the whitespaces
    for (int index = 0; index < line_length; index++)
        //index iterates ove length of line
    {
        if (!isspace(line[index]))
        //if index is not a whitespace
        {
            new_start_ptr = line + index;
            break;
        }
    }

    //loop that walks backwards
    for (int index = line_length - 1; 0 <= index; index--)
    {
        if (isspace(line[index]))
        {
            continue;
        }
        else
        {
            line[index + 1] = '\0'; 
            break;
        }
        
    }
    
    

    return new_start_ptr;
}

void repl_run(FILE *in, FILE *out) {
    /* TODO:
     *   char *line = NULL; size_t cap = 0;
     *   loop:
     *     fputs(PROMPT, out); fflush(out);
     *     if (getline(&line, &cap, in) == -1) break;     <- EOF
     *     strip_newline(line); trimmed = trim(line);
     *     if (*trimmed == '\0') continue;                 <- blank
     *     if (handle_line(trimmed, out) == OUTCOME_EXIT) break;
     *   free(line);                                       <- exactly once
     */
    (void)in; (void)out;
}
