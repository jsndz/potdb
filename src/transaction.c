#include<stdio.h>
#include<transaction.h>
#include<stdlib.h>

int getTxID(){
    return transaction_id++;
}

Transaction *Newtransaction(){
    Transaction t;
    t.ID  = getTxID();
    t.state = ACTIVE;
    return &t;
}