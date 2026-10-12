#ifndef REPL_H
#define REPL_H

#include <stddef.h>
#include <sys/types.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "btree.h"
#include "wal.h"

typedef struct InputBuffer
{
    char *buffer;
    size_t buffer_length;
    ssize_t input_length;
} InputBuffer;

typedef enum
{
    META_COMMAND_SUCCESS,
    META_COMMAND_UNRECOGNIZED_COMMAND
} MetaCommandResult;

InputBuffer *new_input_buffer();
void read_input(InputBuffer *inputBuffer);
void close_input_buffer(InputBuffer *inputBuffer);
void print_prompt();
void print_constants();
void indent(uint32_t level);
MetaCommandResult do_meta_command(InputBuffer *input_buffer, Table *table,Logger *logger);

#endif // REPL_H
