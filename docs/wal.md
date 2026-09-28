Wal should be structured as binary file with this pattern
Since there is a fixed size for Row Payload size will be fixed
the append will happen with a whole page i.e, whole btree node
even for a single log


Usually, no. A non-transactional operation does not need a transaction ID.
No. A normal SELECT should not be written to WAL because it doesn't modify database state.