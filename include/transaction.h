#ifndef TRANSACTION_H
#define TRANSACTION_H

#include <stdbool.h>
#include <stdint.h>
#include "pager.h"
#include "btree.h"
#include "statement.h"

typedef enum
{
    ACTIVE,
    PARTIALLY_COMMITTED,
    COMMITTED,
    FAILED,
    ABORTED
} TransactionStates;

typedef struct {
    int ID;
    TransactionStates state;
    uint32_t changed_page;
    uint32_t initial_num_pages;
} Transaction;

extern int transaction_id;
extern bool IN_TRANSACTION;
extern Transaction *txn;

ExecuteResult txn_begin(Table *table, Logger *logger);
ExecuteResult txn_commit(Table *table, Logger *logger);
ExecuteResult txn_rollback(Table *table, Logger *logger);
void txn_record_page_change(uint32_t page_num);

void start_transaction(Pager *pager);
Transaction *GetCurrentTransaction(void);

#endif