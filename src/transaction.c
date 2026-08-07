#include<stdio.h>
#include<transaction.h>
#include<stdlib.h>


int transaction_id = 1;
bool IN_TRANSACTION = false;
Transaction *txn = NULL;

int getTxID(){
    return transaction_id++;
}

Transaction *Newtransaction(){
    Transaction t;
    t.ID  = getTxID();
    t.state = ACTIVE;
    return &t;
}

Transaction *GetCurrentTransaction(){
    return txn;
}

void start_transaction(){
    t = Newtransaction();
    IN_TRANSACTION = true;
    txn = malloc(sizeof(Transaction));
    txn = t;
    return ;
}