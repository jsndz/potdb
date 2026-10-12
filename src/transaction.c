#include <stdio.h>
#include <stdlib.h>
#include "transaction.h"
#include "node.h"

int transaction_id = 1;
bool IN_TRANSACTION = false;
Transaction *txn = NULL;

int getTxID() {
    return transaction_id++;
}

Transaction *Newtransaction(Pager *pager) {
    Transaction *t = malloc(sizeof(Transaction));
    if (t == NULL) {
        printf("Unable to allocate memory for transaction.\n");
        exit(EXIT_FAILURE);
    }
    t->ID = getTxID();
    t->state = ACTIVE;
    t->changed_page = INVALID_PAGE_NUM;
    t->initial_num_pages = pager ? pager->num_pages : 0;
    return t;
}

Transaction *GetCurrentTransaction() {
    return txn;
}

void start_transaction(Pager *pager) {
    if (txn != NULL) {
        free(txn);
        txn = NULL;
    }
    txn = Newtransaction(pager);
    IN_TRANSACTION = true;
}

ExecuteResult txn_begin(Table *table) {
    start_transaction(table->pager);
    return EXECUTE_SUCCESS;
}

ExecuteResult txn_commit(Table *table) {
    Transaction *t = GetCurrentTransaction();
    if (!IN_TRANSACTION || t == NULL) {
        printf("No active transaction to commit.\n");
        return EXECUTE_SUCCESS;
    }

    t->state = PARTIALLY_COMMITTED;

    // Flush dirty pages to disk (FORCE policy).
    // Note: When WAL is implemented, this is where wal_log_commit() & wal_flush()
    // will be called, replacing the page flush (NO-FORCE policy).
    pager_flush_all(table->pager);

    t->state = COMMITTED;
    IN_TRANSACTION = false;
    free(t);
    txn = NULL;
    return EXECUTE_SUCCESS;
}

ExecuteResult txn_rollback(Table *table) {
    Transaction *t = GetCurrentTransaction();
    Pager *pager = table->pager;

    if (!IN_TRANSACTION || t == NULL) {
        printf("No active transaction to rollback.\n");
        return EXECUTE_SUCCESS;
    }

    // Restore pages modified during this transaction.
    // Note: When WAL is implemented, this will be replaced by replaying undo records.
    if (t->changed_page != INVALID_PAGE_NUM) {
        for (uint32_t p = t->changed_page; p < t->initial_num_pages; p++) {
            pager_reload_page(pager, p);
        }
    }

    pager_truncate(pager, t->initial_num_pages);

    t->state = ABORTED;
    IN_TRANSACTION = false;
    free(t);
    txn = NULL;
    return EXECUTE_SUCCESS;
}

void txn_record_page_change(uint32_t page_num) {
    Transaction *t = GetCurrentTransaction();
    if (IN_TRANSACTION && t != NULL) {
        if (page_num < t->changed_page) {
            t->changed_page = page_num;
        }
    }
}