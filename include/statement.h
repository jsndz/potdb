#ifndef STATEMENT_H
#define STATEMENT_H

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "row.h"
#include "btree.h"

typedef struct InputBuffer InputBuffer;

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
    INSERT_STATEMENT,
    TRANSACTION_BEGIN,
    TRANSACTION_COMMIT,
} StatementType;

typedef struct
{
    StatementType type;
    Row row_to_insert;
} Statement;

PrepareResult prepare_statement(InputBuffer *inputBuffer, Statement *statement);
PrepareResult prepare_insert(InputBuffer *input_buffer, Statement *statement);
ExecuteResult execute_statement(Statement *statement, Table *table);
ExecuteResult execute_insert(Statement *statement, Table *table);
ExecuteResult execute_select(Statement *statement, Table *table);

#endif // STATEMENT_H
