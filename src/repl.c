#define _POSIX_C_SOURCE 200809L
#include "repl.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define PROMPT "minidb> "

typedef enum { OUTCOME_CONTINUE, OUTCOME_EXIT } Outcome;



/* Handles one non-blank, trimmed input_string. */
static Outcome handle_line(const char *input_string, FILE *output_stream) {
    /* TODO: leading '.' -> meta-command
     *       otherwise    -> we can't parse SQL yet; say so
     */
    if (input_string[0] == '.')
    {   char *exit_command = ".exit";
        int exiting = strcmp(input_string, exit_command);
        if (exiting == 0)
        {
            return OUTCOME_EXIT;   
        }
    }
    else
    {
        fprintf(output_stream, "Cant parse sql.\n");
        return 0;
    }
    
    
    (void)input_string; (void)output_stream;
    return OUTCOME_CONTINUE;
}

/* Removes a trailing '\n' (and '\r' if present) input_stream place. */
static void strip_newline(char *input_string) {
    /* TODO */

    size_t string_end_index = strcspn(input_string, "\n\r");
    input_string[string_end_index] = '\0';
}

/* Returns a pointer to the first non-space char; trims trailing space input_stream place. */
static char *trim(char *input_string) {
    /* TODO */
    int line_length = (int)strlen(input_string);
    char *new_start_ptr = input_string + line_length;

    //move new start ptr past the whitespaces
    for (int index = 0; index < line_length; index++)
        //index iterates ove length of input_string
    {
        if (!isspace(input_string[index]))
        //if index is not a whitespace
        {
            new_start_ptr = input_string + index;
            break;
        }
    }

    //loop that walks backwards
    for (int index = line_length - 1; 0 <= index; index--)
    {
        if (isspace(input_string[index]))
        {
            continue;
        }
        else
        {
            input_string[index + 1] = '\0'; 
            break;
        }
    }
    return new_start_ptr;
}

void repl_run(FILE *input_stream, FILE *output_stream) {
    /* TODO:
     *   char *input_string = NULL; size_t cap = 0;
     *   loop:
     *     fputs(PROMPT, output_stream); fflush(output_stream);
     *     if (getline(&input_string, &cap, input_stream) == -1) break;     <- EOF
     *     strip_newline(input_string); trimmed = trim(input_string);
     *     if (*trimmed == '\0') continue;                 <- blank
     *     if (handle_line(trimmed, output_stream) == OUTCOME_EXIT) break;
     *   free(input_string);                                       <- exactly once
     */
    
     //these 2 vars are used input_stream getline()
     // input_string = address of data
     //cap = size of buffer
     char *input_string = NULL;
     size_t buffer_len = 0;
     int starts = 0;

     while (starts == 0)
     {  //writes prompt output_stream's file stream
        fputs(PROMPT, output_stream);
        //write all data cur in stream
        fflush(output_stream);

        //begin retriving the input

     }
     

     



    (void)input_stream; (void)output_stream;
}
