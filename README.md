# potDB

**potDB** is a lightweight key-value database built in C for recording and querying an object's spatial coordinates over time.

$$\text{Time} \rightarrow \text{Position}$$

It efficiently stores and retrieves where an object was located `(x, y)` at a given point in time using an on-disk B-Tree index.

---

## Architecture

The storage and query processing engine follows a layered architecture:

`Cursor -> Table -> Pager -> File`

- **Key-Value Model**: The primary key is a UNIX timestamp (`time_t`, in seconds) and the value is a 2D coordinate `Position { int x, int y; }`.
- **Pages**: Data is partitioned into fixed 4KB pages managed by a Pager.
- **B-Tree**:
  - **Leaf Nodes**: Store sorted key-value pairs (cells) with pointers to sibling leaf nodes for sequential scans.
  - **Internal Nodes**: Guide searches down the tree hierarchy with keys and child page pointers.
- **Transactions & Persistence**: Transaction control (`begin`, `commit`, `rollback`) with page-level cache management.

---

## Installation & Build

### Prerequisites
- GCC / Clang
- GNU Make

### Build Steps

1. Clone the repository:
   ```bash
   git clone https://github.com/jsndz/potdb
   cd potdb
   ```

2. Compile using `make`:
   ```bash
   make
   ```
   *(Or compile manually)*:
   ```bash
   gcc -Wall -Wextra -Iinclude -std=c99 -D_GNU_SOURCE src/*.c -o potdb
   ```

---

## Running potDB

Start the database by providing the path to a database file (created automatically if it does not exist):

```bash
./potdb mydb.db
```

---

## Usage

### 1. Data Operations

- **Insert a Position**
  Inserts coordinate `(x, y)` mapped to the current UNIX timestamp:
  ```sql
  insert 10 20
  ```

- **Query Stored Positions**
  Scans the B-Tree and prints all records chronologically in `(timestamp, x, y)` format:
  ```sql
  select
  ```

### 2. Transaction Commands

- **Begin a transaction**:
  ```sql
  begin
  ```

- **Commit changes**:
  ```sql
  commit
  ```

- **Roll back changes**:
  ```sql
  rollback
  ```

### 3. Meta Commands

Commands prefixed with `.` provide debugging and inspection tools:

- `.btree`: Prints a textual visualization of the B-Tree structure and keys.
- `.constants`: Displays node headers, cell sizes, and page capacities.
- `.exit`: Flushes dirty pages, closes file descriptors, and exits the REPL.

---

## Documentation

Additional design logs and implementation notes can be found in the [`docs/`](docs/) directory:
- [`docs/logs.md`](docs/logs.md): Step-by-step implementation logs and phase notes.
- [`docs/transactions.md`](docs/transactions.md): Transaction design and recovery mechanisms.
- [`docs/wal.md`](docs/wal.md): Write-Ahead Logging (WAL) specifications.
