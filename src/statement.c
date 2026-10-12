#include "statement.h"
#include "repl.h"
#include "transaction.h"
#include "node.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "position.h"
#include <time.h>
PrepareResult prepare_insert(InputBuffer *input_buffer, Statement *statement)
{
    statement->type = INSERT_STATEMENT;
    const time_t timestamp = time(NULL);
    Position *pos = get_position(input_buffer);
    statement->row_to_insert.timestamp = timestamp;
    statement->row_to_insert.pos = *pos;
    free(pos);
    return PREPARE_SUCCESS;
}

PrepareResult prepare_statement(InputBuffer *inputBuffer, Statement *statement)
{
    if (strncmp(inputBuffer->buffer, "insert", 6) == 0)
    {
        return prepare_insert(inputBuffer, statement);
    }
    else if (strncmp(inputBuffer->buffer, "select", 6) == 0)
    {
        statement->type = SELECT_STATEMENT;
        return PREPARE_SUCCESS;
    }
    else if (strncmp(inputBuffer->buffer, "begin", 5) == 0)
    {
        statement->type = TRANSACTION_BEGIN;
        return PREPARE_SUCCESS;
    }
    else if (strncmp(inputBuffer->buffer, "commit", 6) == 0)
    {
        statement->type = TRANSACTION_COMMIT;
        return PREPARE_SUCCESS;
    }
    else if (strncmp(inputBuffer->buffer, "rollback", 8) == 0)
    {
        statement->type = TRANSACTION_ROLLBACK;
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
    time_t key_to_insert = row_to_insert->timestamp;
    Cursor *cursor = table_find(table, key_to_insert);

    void *node = get_page(table->pager, cursor->page_num);
    uint32_t num_cells = *leaf_node_num_cells(node);

    txn_record_page_change(cursor->page_num);

    if (cursor->cell_num < num_cells)
    {
        time_t key_at_index = *leaf_node_key(node, cursor->cell_num);
        if (key_at_index == key_to_insert)
        {
            printf("Duplicate key error: %ld\n", (long)key_to_insert);
            free(cursor);
            return EXECUTE_DUPLICATE_KEY;
        }
    }

    leaf_node_insert(cursor, row_to_insert->timestamp, row_to_insert);

    if (!IN_TRANSACTION)
    {
        pager_flush(cursor->table->pager, cursor->page_num);
    }
    
    free(cursor);
    return EXECUTE_SUCCESS;
}

ExecuteResult execute_select(Statement *statement, Table *table)
{
    (void)statement;
    Cursor *cursor = table_start(table);
    Row row;
    while (!(cursor->end_of_table))
    {
        deserialize_row(cursor_value(cursor), &(row));
        print_row(&(row));
        cursor_advance(cursor);
    }
    free(cursor);
    return EXECUTE_SUCCESS;
}

ExecuteResult execute_statement(Statement *statement, Table *table,Logger *logger)
{
    switch (statement->type)
    {
    case SELECT_STATEMENT:
        return execute_select(statement, table);
    case INSERT_STATEMENT:
        return execute_insert(statement, table);
    case TRANSACTION_BEGIN:
        return txn_begin(table);
    case TRANSACTION_COMMIT:
        return txn_commit(table);
    case TRANSACTION_ROLLBACK:
        return txn_rollback(table);
    default:
        return EXECUTE_SUCCESS;
    }
}

