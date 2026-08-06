#ifndef TRANSACTION_H
#define TRANSACTION_H
int transaction_id =1;

typedef enum  
{
    ACTIVE,
    PARTIALLY_COMMITTED,
    COMMITED,
    FAILED,
    ABORTED
}TransactionStates ;

typedef struct {
   int ID;
   TransactionStates state;
} Transaction;

#endif