Wal should be strcuted as binary file with this pattern
Since there is a fixed size for Row Payload size will be fixed
+----------+---------+--------+---------+----------+
| LSN      | Txn ID  | Type   | Page ID | Payload  |
+----------+---------+--------+---------+----------+