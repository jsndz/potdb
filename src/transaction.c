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