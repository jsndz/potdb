#include <stdio.h>
#include <stdlib.h>
#include "repl.h"
#include "statement.h"
#include "btree.h"
#include "transaction.h"
#include "wal.h"


int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("Need to enter the File name.\n");
        exit(EXIT_FAILURE);
    }
    char *db_filename = argv[1];
    char *log_filename=  argv[2];
    Table *table = db_open(db_filename);
    Logger *logger = log_open(log_filename);
    InputBuffer *inputBuffer = new_input_buffer();

    while (true)
    {
        print_prompt();
        read_input(inputBuffer);

        if (inputBuffer->buffer[0] == '.')
        {
            switch (do_meta_command(inputBuffer, table,logger))
            {
            case (META_COMMAND_SUCCESS):
                continue;
            case (META_COMMAND_UNRECOGNIZED_COMMAND):
                printf("Unrecognized command '%s'\n", inputBuffer->buffer);
                continue;
            }
        }

        Statement statement;
        switch (prepare_statement(inputBuffer, &statement))
        {
        case PREPARE_SUCCESS:
            break;
        case PREPARE_NEGATIVE_ID:
            printf("ID can't be negative.\n");
            continue;
        case PREPARE_UNRECOGNIZED_STATEMENT:
            printf("Unrecognized command at the start %s \n", inputBuffer->buffer);
            continue;
        case PREPARE_STRING_TOO_LONG:
            printf("string is too long.\n");
            continue;
        case PREPARE_SYNTAX_ERROR:
            printf("Syntax error. Could not parse statement.\n");
            continue;
        }

        switch (execute_statement(&statement, table, logger))
        {
        case EXECUTE_SUCCESS:
            printf("Executed.\n");
            break;
        case EXECUTE_DUPLICATE_KEY:
            printf("Key Already Exists.\n");
            break;
        case EXECUTE_TABLE_FULL:
            printf("Error: Table full.\n");
            break;
        }
    }

    return 0;
}
