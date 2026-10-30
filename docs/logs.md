
## 23/10/26
### Resigning Log

Lets start with how the project was before,
It was a sqlite db clone, which was using b+ tree to store data and just fixed table structure of id, name and email.
Then had the idea of adding transactions, WAL etc.
When i was implementing WAL, I thought why not make it a proper DB.
So I thought lets make it store time-stamp to position Key-Value DB.
Then i started working on WAL and when implementing rollback, I realised for a fast time-series database TXN the way I am implementing is pretty useless.

So Lets redesign PotDB.

## 29/10/26
## an append log which is chunked 
Based on the simple idea of time-series data being ordered we are doing a using a append only file to store the data in disk.
This is better than other available option based on how fast it is for read and write.
For crash recovery kept a time based flushing mechanism so that even if the crash occurs we may loose minimum amount of data.
We can configure this such that if you dont want to loose any data you can lower the time for flush.
Also fixed the problem pragma by handling serialisation and deserialisation with byte level conversion functions.

