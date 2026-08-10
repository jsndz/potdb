#include "statement.h"
#include "repl.h"
#include "transaction.h"
#include "node.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

PrepareResult prepare_insert(InputBuffer *input_buffer, Statement *statement)
{
    statement->type = INSERT_STATEMENT;
    const char *keyword = strtok(input_buffer->buffer, " ");
    const char *id_STRING = strtok(NULL, " ");
    const char *username = strtok(NULL, " ");
    const char *email = strtok(NULL, " ");

    (void)keyword;

    if (id_STRING == NULL || username == NULL || email == NULL)
    {
        return PREPARE_SYNTAX_ERROR;
    }
    int id = atoi(id_STRING);
    if (id < 0)
    {
        return PREPARE_NEGATIVE_ID;
    }
    if (strlen(username) >= COL_USERNAME_SIZE)
    {
        return PREPARE_STRING_TOO_LONG;
    }
    if (strlen(email) >= COL_EMAIL_SIZE)
    {
        return PREPARE_STRING_TOO_LONG;
    }
    statement->row_to_insert.id = id;
    strcpy(statement->row_to_insert.username, username);
    strcpy(statement->row_to_insert.email, email);
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
    uint32_t key_to_insert = row_to_insert->id;
    Cursor *cursor = table_find(table, key_to_insert);
    Transaction *t = GetCurrentTransaction();

    void *node = get_page(table->pager, cursor->page_num);
    uint32_t num_cells = *leaf_node_num_cells(node);

    if (IN_TRANSACTION && t != NULL)
    {
        if (cursor->page_num < t->changed_page)
        {
            t->changed_page = cursor->page_num;
        }
    }

    if (cursor->cell_num < num_cells)
    {
        uint32_t key_at_index = *leaf_node_key(node, cursor->cell_num);
        if (key_at_index == key_to_insert)
        {
            printf("Duplicate key error: %d\n", key_to_insert);
            free(cursor);
            return EXECUTE_DUPLICATE_KEY;
        }
    }

    leaf_node_insert(cursor, row_to_insert->id, row_to_insert);

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

ExecuteResult execute_begin_transaction(Table *table)
{
    start_transaction(table->pager);
    return EXECUTE_SUCCESS;
}

ExecuteResult execute_commit_transaction(Table *table)
{
    Transaction *t = GetCurrentTransaction();
    if (!IN_TRANSACTION || t == NULL)
    {
        printf("No active transaction to commit.\n");
        return EXECUTE_SUCCESS;
    }

    t->state = PARTIALLY_COMMITTED;

    // FORCE policy: Flush all loaded/modified pages in buffer pool to disk
    for (uint32_t p = 0; p < table->pager->num_pages; p++)
    {
        if (table->pager->pages[p] != NULL)
        {
            pager_flush(table->pager, p);
        }
    }

    t->state = COMMITTED;
    IN_TRANSACTION = false;
    free(t);
    txn = NULL;
    return EXECUTE_SUCCESS;
}

ExecuteResult execute_rollback_transaction(Table *table)
{
    Transaction *t = GetCurrentTransaction();
    Pager *pager = table->pager;

    if (!IN_TRANSACTION || t == NULL)
    {
        printf("No active transaction to rollback.\n");
        return EXECUTE_SUCCESS;
    }

    if (t->changed_page != INVALID_PAGE_NUM)
    {
        for (uint32_t p = t->changed_page; p < t->initial_num_pages; p++)
        {
            if (pager->pages[p] != NULL)
            {
                lseek(pager->file_descriptor, p * PAGE_SIZE, SEEK_SET);
                ssize_t bytes_read = read(pager->file_descriptor, pager->pages[p], PAGE_SIZE);
                (void)bytes_read;
            }
        }
    }

    for (uint32_t p = t->initial_num_pages; p < pager->num_pages; p++)
    {
        if (pager->pages[p] != NULL)
        {
            free(pager->pages[p]);
            pager->pages[p] = NULL;
        }
    }
    pager->num_pages = t->initial_num_pages;

    t->state = ABORTED;
    IN_TRANSACTION = false;
    free(t);
    txn = NULL;
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
    case TRANSACTION_BEGIN:
        return execute_begin_transaction(table);
    case TRANSACTION_COMMIT:
        return execute_commit_transaction(table);
    case TRANSACTION_ROLLBACK:
        return execute_rollback_transaction(table);
    default:
        return EXECUTE_SUCCESS;
    }
}

