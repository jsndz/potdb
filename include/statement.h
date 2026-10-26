#ifndef STATEMENT_H
#define STATEMENT_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "row.h"
#include "table.h"

typedef enum
{
    PREPARE_SUCCESS,
    PREPARE_UNRECOGNIZED_STATEMENT,
    PREPARE_SYNTAX_ERROR,
    PREPARE_NEGATIVE_ID,
    PREPARE_STRING_TOO_LONG
} PrepareResult;

typedef enum
{
    EXECUTE_SUCCESS,
    EXECUTE_TABLE_FULL,
    EXECUTE_DUPLICATE_KEY
} ExecuteResult;

typedef enum
{
    SELECT_STATEMENT,
    INSERT_STATEMENT
} StatementType;

typedef struct Statement
{
    StatementType type;
    Row row_to_insert;
} Statement;

PrepareResult prepare_statement(const char *input, Statement *statement);
PrepareResult prepare_insert(const char *input, Statement *statement);
ExecuteResult execute_statement(Statement *statement, Table *table);
ExecuteResult execute_insert(Statement *statement, Table *table);
ExecuteResult execute_select(Statement *statement, Table *table);

#endif
