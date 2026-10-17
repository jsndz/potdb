# Write-Ahead Logging (WAL) Specification

The Write-Ahead Log (WAL) ensures durability and crash recovery for **potDB** by appending sequential state changes before they are committed to disk.
Wal should be structured as binary file with this pattern
Since there is a fixed size for Row Payload size will be fixed
the append will happen with a whole page i.e, whole btree node
even for a single log


Usually, no. A non-transactional operation does not need a transaction ID.
No. A normal SELECT should not be written to WAL because it doesn't modify database state.
---

## 1. WAL Record Format

The WAL is stored as an append-only binary file. Each log record has a fixed size of **28 bytes** (`LOG_SIZE`):

| Offset (Bytes) | Field | Type | Size | Description |
| :--- | :--- | :--- | :--- | :--- |
| `0 - 3` | `LSN` | `uint32_t` | 4 bytes | Monotonically increasing Log Sequence Number |
| `4 - 7` | `TXN` | `uint32_t` | 4 bytes | Transaction ID (`0` for non-transactional writes) |
| `8 - 11` | `TYPE` | `uint32_t` | 4 bytes | Record type (`LogRecordType` enum) |
| `12 - 27` | `ROW` | `Row` | 16 bytes | Serialized payload (`timestamp` [8B] + `Position { x, y }` [8B]) |

### Record Types (`LogRecordType`)

- `LOG_RECORD_BEGIN` (`0`): Marks the initiation of a transaction (`begin`).
- `LOG_RECORD_COMMIT` (`1`): Marks successful commit of a transaction (`commit`). Serves as the durability boundary.
- `LOG_RECORD_ROLLBACK` (`2`): Marks transaction cancellation (`rollback`).
- `LOG_RECORD_INSERT` (`3`): Data modification record containing the inserted `Row`.

---

## 2. In-Memory WAL Buffer & Flushing

To prevent synchronous random disk I/O on every insert, the [`Logger`](../include/wal.h) buffers writes in RAM:

- **Buffer Size**: 4096 bytes (`WAL_BUFFER_SIZE`), capable of holding up to $146$ records ($4096 / 28$).
- **Flush Triggers**:
  - Automatically flushed when the 4 KB buffer is full.
  - Flushed on non-transactional inserts immediately after execution.
  - Flushed synchronously when a transaction issues `commit`.
  - Flushed when the database shuts down cleanly (`.exit` / `db_close`).

---

## 3. Crash Recovery

Upon opening the WAL file during startup:

1. The file length is checked for corruption (`file_length % LOG_SIZE == 0`).
2. The last record in the log is deserialized to recover the highest allocated `LSN`.
3. In-flight records without a subsequent `LOG_RECORD_COMMIT` are identified and discarded during recovery.