#ifndef TRANSACTION_H
#define TRANSACTION_H

#include <stdbool.h>

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
} Transaction;


extern int transaction_id;
extern bool IN_TRANSACTION;
extern Transaction *t;

#endif