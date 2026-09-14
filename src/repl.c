#define _POSIX_C_SOURCE 200809L
#include "repl.h"

#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define PROMPT "minidb> "

typedef enum { __CONTINUE, __EXIT, __CANTPARSE } EXE_Result;



/* Handles one non-blank, trimmed input_string_buffer. */
static EXE_Result parse_input(const char *input_string_buffer, FILE *output_stream) {
    /* TODO: leading '.' -> meta-command
     *       otherwise    -> we can't parse SQL yet; say so
     */
    if (input_string_buffer[0] == '.')
    {   char *exit_command = ".exit";
        int exiting = strcmp(input_string_buffer, exit_command);
        if (exiting == 0)
        {
            return __EXIT;   
        }
    }
    else
    {
        fprintf(output_stream, "Cant parse sql.\n");
        return __CANTPARSE;
    }
    
    
    (void)input_string_buffer; (void)output_stream;
    return __CONTINUE;
}



/* Removes a trailing '\n' (and '\r' if present) input_stream place. */
static void strip_terminating_char(char *input_string_buffer) {
    /* TODO */

    size_t string_end_index = strcspn(input_string_buffer, "\n\r");
    input_string_buffer[string_end_index] = '\0';
}




/* Returns a pointer to the first non-space char; trims trailing space input_stream place. */
static char *trim(char *input_string_buffer) {
    
    int line_length = (int)strlen(input_string_buffer);
    char *new_start_ptr = input_string_buffer + line_length;

    //move new start ptr past the whitespaces
    for (int index = 0; index < line_length; index++)
        //index iterates ove length of input_string_buffer
    {
        if (!isspace(input_string_buffer[index]))
        //if index is not a whitespace
        {
            new_start_ptr = input_string_buffer + index;
            break;
        }
    }

    //loop that walks backwards
    for (int index = line_length - 1; 0 <= index; index--)
    {
        if (isspace(input_string_buffer[index]))
        {
            continue;
        }
        else
        {
            input_string_buffer[index + 1] = '\0'; 
            break;
        }
    }
    return new_start_ptr;
}





void repl_run(FILE* input_stream, FILE* output_stream) {
    /* TODO:
     *   char *input_string_buffer = NULL; size_t cap = 0;
     *   loop:
     *     fputs(PROMPT, output_stream); fflush(output_stream);
     *     if (getline(&input_string_buffer, &cap, input_stream) == -1) break;     <- EOF
     *     strip_terminating_char(input_string_buffer); trimmed = trim(input_string_buffer);
     *     if (*trimmed == '\0') continue;                 <- blank
     *     if (parse_input(trimmed, output_stream) == OUTCOME_EXIT) break;
     *   free(input_string_buffer);                                       <- exactly once
     */
    
     //buffer init for getline()  
    char *input_string_buffer = NULL;

     //var init for getline()
    size_t buffer_size = 0;

     //while loop trigger
    int starts = 0;

    while (starts == 0)
    {  //writes prompt output_stream's file stream
        fputs(PROMPT, output_stream);
        //write all data cur in stream
        fflush(output_stream);

        //getline() usage:= read text string
        //
        if (getline(&input_string_buffer, &buffer_size, input_stream) == EOF)
        {
            break;
        }
        //getline returns the terminating char
        strip_terminating_char(input_string_buffer);

        //trim any extra spaces at start/end of bufer
        char* trimmed_str_buffer = trim(input_string_buffer);

        //guard against empty imputs
        if (*trimmed_str_buffer == '\0')
        {
            continue;
        }

        //syntax check
        if (parse_input(trimmed_str_buffer, output_stream) == __EXIT)
        {
            break;
        }
    }
    free(input_string_buffer);

     

     



    (void)input_stream; (void)output_stream;
}
