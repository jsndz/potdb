# potDB

**potDB** is a lightweight, page-based embedded key-value database written in C for recording and querying spatial coordinates over time.

$$\text{Time} \rightarrow \text{Position}$$

It stores and retrieves an object's location `(x, y)` at any given point in time using an on-disk B+ Tree index with a Write-Ahead Log (WAL) and transaction management.

---

## Architecture

The storage and query processing engine follows a layered architecture:

$$\text{REPL} \longrightarrow \text{Statement Engine} \longrightarrow \text{Cursor / Table} \longrightarrow \text{B+ Tree} \longrightarrow \text{Pager} \longrightarrow \text{Disk}$$

### 1. Key-Value Data Model
- **Primary Key**: UNIX timestamp (`time_t`, 8 bytes on 64-bit systems), representing seconds since the epoch.
- **Value**: Spatial coordinate struct [`Position`](include/position.h):
  ```c
  typedef struct {
      int x;
      int y;
  } Position;
  ```
- **Row**: Key and value packed into a 16-byte fixed-size record (`Row { time_t timestamp; Position pos; }`).

### 2. Node Representation in Memory (Zero-Copy Paging)
Each B+ Tree node corresponds to a single **4096-byte (4 KB) page** managed by the [`Pager`](include/pager.h). Nodes are not declared as traditional C structs with pointers; instead, fields and cells are laid out contiguously at exact byte offsets and accessed via pointer arithmetic. This layout matches the on-disk binary format 1:1, requiring no serialization overhead.

- **Common Header (6 bytes)**:
  - `node_type` (`uint8_t`, 1 byte): `NODE_INTERNAL` (`0`) or `NODE_LEAF` (`1`).
  - `is_root` (`uint8_t`, 1 byte): `1` if the node is root, `0` otherwise.
  - `parent_pointer` (`uint32_t`, 4 bytes): Page number of the parent node.

- **Leaf Node Layout**:
  - **Header (14 bytes)**: Common header (6B) + `num_cells` (`uint32_t`, 4B) + `next_leaf` (`uint32_t`, 4B).
  - **Cells Body**: Array of 24-byte cells (`time_t key` [8B] + `Row value` [16B]).
  - **Capacity**: Holds up to **170 cells** per 4 KB page (`LEAF_NODE_MAX_CELLS`).
  - **Linked List**: Sibling leaves are linked via `next_leaf` page numbers for fast chronological sequential scans.

- **Internal Node Layout**:
  - **Header (14 bytes)**: Common header (6B) + `num_keys` (`uint32_t`, 4B) + `right_child` (`uint32_t`, 4B).
  - **Cells Body**: Array of 12-byte cells (`child_page_num` [4B] + `max_key` [8B]).
  - **Capacity**: Configured for up to **3 keys** (`INTERNAL_NODE_MAX_KEYS`), pointing to up to 4 children (children $0 \dots K-1$ and `right_child`).

### 3. B+ Tree Operations
- **Search**: Binary search within nodes and descent through child pointers in $O(\log N)$ time.
- **Insert**: Cells are inserted in sorted order. If a leaf exceeds 170 cells, it splits (86 left / 85 right) and pushes routing keys up to its parent internal node, creating a new root if required.
- **Delete**: 
  - Locates the key via binary search and shifts trailing cells left.
  - Recursively updates routing keys in ancestor nodes if the deleted entry was the node's maximum key.
  - Checks for underflow (`< LEAF_NODE_MIN_CELLS` or `< INTERNAL_NODE_MIN_KEYS`).
  - Borrows (redistributes) from left or right sibling if a sibling has surplus cells.
  - Merges (coalesces) with a sibling if neither can lend, updating leaf sibling links and removing the vacated child from the parent.
  - Shrinks tree height when the root internal node is reduced to a single child.

### 4. Transactions & Write-Ahead Logging (WAL)
- **ACID Support**: Supports `begin`, `commit`, and `rollback` transaction semantics.
- **Binary WAL**: Serializes operations to a fixed-format WAL file (`LSN` [4B] + `txn_id` [4B] + `type` [4B] + `row` [16B] = 28 bytes per log record).
- **Rollback Tracking**: Tracks modified pages in memory during a transaction, allowing uncommitted page modifications to be discarded or restored from disk on rollback.

---

## Installation & Build

### Prerequisites
- GCC or Clang (supporting C99)
- GNU Make

### Build Steps

1. Clone the repository:
   ```bash
   git clone https://github.com/jsndz/potdb.git
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

Start potDB by specifying paths for both the database file and the WAL file (both will be created automatically if they do not exist):

```bash
./potdb mydb.db mywal.log
```

---

## Command Reference

### 1. Data Operations

- **Insert a Coordinate**:
  Maps 2D position `(x, y)` to the current UNIX timestamp:
  ```sql
  insert 10 20
  ```

- **Delete by Timestamp**:
  Deletes the record matching the specified timestamp key:
  ```sql
  delete 1729000000
  ```

- **Query Chronological Positions**:
  Performs an in-order scan across the B+ Tree leaf nodes, printing all records in `(timestamp, x, y)` format:
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

### 3. Meta / Debugging Commands

Commands prefixed with `.` provide engine introspection:

- **`.btree`**: Prints an indented structural visualization of the entire B+ Tree hierarchy (internal nodes and leaf keys).
- **`.constants`**: Displays internal constants, node header offsets, cell sizes, and page capacities.
- **`.exit`**: Flushes uncommitted dirty pages, closes file descriptors, and cleanly exits the REPL.

---

## Project Structure

```
├── include/
│   ├── btree.h         # Table, Cursor, and B-Tree function declarations (search, insert, delete)
│   ├── lock.h          # Concurrency and locking structures
│   ├── node.h          # Node layout, byte offsets, capacities, and accessor declarations
│   ├── pager.h         # Page cache, 4KB page allocation, and file I/O
│   ├── position.h      # Position struct (x, y) definition and parser
│   ├── repl.h          # REPL input buffer and meta-command handler
│   ├── row.h           # Row serialization, deserialization, and print utilities
│   ├── statement.h     # Statement parsing, AST types, and execution dispatcher
│   ├── transaction.h   # Transaction lifecycle (begin, commit, rollback) and page tracking
│   └── wal.h           # Write-Ahead Log record structures and serialization
├── src/
│   ├── btree.c         # B+ Tree search, split/insert, delete, underflow/merge, and print
│   ├── main.c          # Entry point and REPL main loop
│   ├── node.c          # Raw memory offset accessors for leaf and internal nodes
│   ├── pager.c         # File descriptor management, page fetching, and flushing
│   ├── position.c      # Coordinate parsing from input
│   ├── repl.c          # REPL utilities and meta commands (.btree, .constants, .exit)
│   ├── row.c           # Row serialization and console formatting
│   ├── statement.c     # Statement preparation and execution dispatch
│   ├── transaction.c   # Transaction state and rollback page restoration
│   └── wal.c           # WAL file management, log serialization, and append/flush
├── docs/
│   ├── btree.md        # Detailed B+ Tree layout, splitting, and deletion algorithms
│   ├── design.md       # Architectural decisions (WAL, I/O patterns, Force vs No-Force)
│   ├── logs.md         # Implementation notes and phase history
│   ├── transactions.md # ARIES framework policies, atomicity, and recovery notes
│   └── wal.md          # Binary WAL format and durability model
├── Makefile            # Build configuration
└── README.md           # Project documentation
```

---

## Detailed Documentation

Comprehensive design documents and technical deep dives are available in the [`docs/`](docs/) directory:
- [`docs/btree.md`](docs/btree.md): Memory layouts, split algorithms, underflow handling, and sibling coalescing.
- [`docs/design.md`](docs/design.md): Storage engine architecture, sequential vs. random I/O, and write amplification.
- [`docs/transactions.md`](docs/transactions.md): ACID policies, No-Steal/Force vs. WAL No-Force, and recovery.
- [`docs/wal.md`](docs/wal.md): Write-Ahead Log binary specification and durability guarantees.
