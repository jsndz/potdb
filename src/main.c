#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "statement.h"
#include "table.h"

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <db_file> [insert <timestamp> <x> <y> | select]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    char *db_filename = argv[1];
    Table *table = db_open(db_filename);

    if (argc >= 3)
    {
        if (strcmp(argv[2], "insert") == 0)
        {
            if (argc < 6)
            {
                fprintf(stderr, "Usage: %s %s insert <timestamp> <x> <y>\n", argv[0], argv[1]);
                db_close(table);
                return 1;
            }

            long timestamp = atol(argv[3]);
            if (timestamp < 0)
            {
                fprintf(stderr, "ID can't be negative.\n");
                db_close(table);
                return 1;
            }

            Row row;
            row.timestamp = (time_t)timestamp;
            row.pos.x = atoi(argv[4]);
            row.pos.y = atoi(argv[5]);

            table_append_row(table, &row);
        }
        else if (strcmp(argv[2], "select") == 0)
        {
            table_select_all(table);
        }
        else
        {
            fprintf(stderr, "Unrecognized command: %s\n", argv[2]);
            db_close(table);
            return 1;
        }

        db_close(table);
        return 0;
    }

    // Stdin stream mode (piping / batch input)
    char *line = NULL;
    size_t len = 0;
    ssize_t read_bytes;

    while ((read_bytes = getline(&line, &len, stdin)) != -1)
    {
        while (read_bytes > 0 && (line[read_bytes - 1] == '\n' || line[read_bytes - 1] == '\r'))
        {
            line[--read_bytes] = '\0';
        }
        if (read_bytes == 0)
        {
            continue;
        }

        Statement statement;
        switch (prepare_statement(line, &statement))
        {
        case PREPARE_SUCCESS:
            break;
        case PREPARE_NEGATIVE_ID:
            fprintf(stderr, "ID can't be negative.\n");
            continue;
        case PREPARE_UNRECOGNIZED_STATEMENT:
            fprintf(stderr, "Unrecognized command: %s\n", line);
            continue;
        case PREPARE_SYNTAX_ERROR:
            fprintf(stderr, "Syntax error. Could not parse statement.\n");
            continue;
        case PREPARE_STRING_TOO_LONG:
            fprintf(stderr, "String is too long.\n");
            continue;
        }

        execute_statement(&statement, table);
    }

    free(line);
    db_close(table);
    return 0;
}
