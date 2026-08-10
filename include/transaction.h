#ifndef TRANSACTION_H
#define TRANSACTION_H

#include <stdbool.h>
#include <stdint.h>
#include "pager.h"

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

void start_transaction(Pager *pager);
Transaction *GetCurrentTransaction(void);

#endif