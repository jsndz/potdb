#include "statement.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

PrepareResult prepare_insert(const char *input, Statement *statement)
{
    statement->type = INSERT_STATEMENT;

    long timestamp;
    int x;
    int y;
    int args_assigned = sscanf(input, "insert %ld %d %d", &timestamp, &x, &y);
    if (args_assigned < 3)
    {
        return PREPARE_SYNTAX_ERROR;
    }

    if (timestamp < 0)
    {
        return PREPARE_NEGATIVE_ID;
    }

    statement->row_to_insert.timestamp = (time_t)timestamp;
    statement->row_to_insert.pos.x = x;
    statement->row_to_insert.pos.y = y;
    return PREPARE_SUCCESS;
}

PrepareResult prepare_statement(const char *input, Statement *statement)
{
    if (strncmp(input, "insert", 6) == 0)
    {
        return prepare_insert(input, statement);
    }
    else if (strncmp(input, "select", 6) == 0)
    {
        statement->type = SELECT_STATEMENT;
        return PREPARE_SUCCESS;
    }
    else
    {
        return PREPARE_UNRECOGNIZED_STATEMENT;
    }
}

ExecuteResult execute_insert(Statement *statement, Table *table)
{
    Row *row_to_insert = &(statement->row_to_insert);
    table_append_row(table, row_to_insert);
    return EXECUTE_SUCCESS;
}

ExecuteResult execute_select(Statement *statement, Table *table)
{
    (void)statement;
    table_select_all(table);
    return EXECUTE_SUCCESS;
}

ExecuteResult execute_statement(Statement *statement, Table *table)
{
    switch (statement->type)
    {
    case SELECT_STATEMENT:
        return execute_select(statement, table);
    case INSERT_STATEMENT:
        return execute_insert(statement, table);
    default:
        return EXECUTE_SUCCESS;
    }
}
