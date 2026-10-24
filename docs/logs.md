
## 23/10/26
### Resigning Log

Lets start with how the project was before,
It was a sqlite db clone, which was using b+ tree to store data and just fixed table structure of id, name and email.
Then had the idea of adding transactions, WAL etc.
When i was implementing WAL, I thought why not make it a proper DB.
So I thought lets make it store time-stamp to position Key-Value DB.
Then i started working on WAL and when implementing rollback, I realised for a fast time-series database TXN the way I am implementing is pretty useless.

So Lets redesign PotDB.
