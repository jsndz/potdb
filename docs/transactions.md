Transaction is a set of database commands to run together considered as one command.
Transactions needs to follow the properties of db: ACID

The Working of transaction is best explained in stages

Consider a Transaction:

First it start with 

BEGIN - which puts state into ACTIVE
and all the db statements are put in the buffer memory i.e., RAM

COMMIT - which puts in COMMITING state

checks all the transactions, locks and adds it to the disk

COMMITTED - The transaction is complete

ROLLBACK / ABORT - 

If transaction fails changes are rolled back

we need to consider ACID, so there are 2 policies for handling this from aries framework
atomicity: 
Steal or No Steal
Steal - Allow dirty commits to be added to the disk to free up RAM
no steal - dont allow dirty commits  to the disk
During the transaction 
durability:
Force or No Force
Force: On commit, all dirty pages modified by the transaction must be synchronously written to disk before the commit returns.
When a transaction commits, all its modified (dirty) pages must be written to disk.
No-Force: Commits return as soon as the change is recorded in memory/log, deferring disk writes of actual datapages.

  ### Approach 1: Strict No-Steal / Force (Memory-Bound In-Place Updates)

  In a pure No-Steal / Force architecture:

  • How Atomicity works (No-Steal): Uncommitted updates are held exclusively in RAM. Disk blocks are never overwritten with
  uncommitted data. If a transaction aborts or the system crashes, uncommitted changes in RAM are simply wiped out. The disk
  content remains completely untouched and consistent.
  • How Durability works (Force): When a transaction commits, the system pauses and synchronously flushes every modified data
  page in RAM directly to disk. Once all disk writes finish, the transaction is marked complete.

  #### Trade-offs:

  • ❌ Slow Commits: Requires synchronous, non-sequential disk I/O at commit time.
  • ❌ RAM Constraints: Transaction size is capped by available RAM since uncommitted pages cannot be swapped to disk.
  ──────
  ### Approach 2: Shadow Paging / Copy-on-Write (COW)

  Used by engines like LMDB, CouchDB, and file systems like ZFS/Btrfs, Shadow Paging eliminates the need for a separate WAL
  log file by making storage immutable.

           [ Root Pointer A ] (Committed)      [ Root Pointer B ] (Pending Commit)
                  |                                   |
            +-----+-----+                       +-----+-----+
            |           |                       |           |
         [ Page 1 ]   [ Page 2 ]             [ Page 1 ]   [ Page 2' ] (New Copy)

  1. Writing Data: Data is organized in a tree structure (like a B-tree). When a page needs modification, the database does
  not update it in place. Instead, it allocates a new disk block and writes a modified copy of the page (Page 2').
  2. Path Updating: Parent pages up to the root are also copied to point to the new disk addresses.
  3. Commit (Atomic Switch): A transaction is committed by updating a single Root Pointer block on disk to point to the new
  tree root. Disk controllers guarantee single-block updates are atomic.
  4. Abort / Recovery: If a crash happens before updating the Root Pointer, the database boots up, reads the old Root Pointer,
  and ignores the newly allocated pages.

  #### Trade-offs:

  • 🟢 Instant Recovery: Crash recovery takes zero time; the system just opens the last committed root pointer.
  • ❌ Fragmentation: Pages become fragmented across disk over time.
  • ❌ Write Amplification: Modifying a leaf node requires duplicating every ancestor page up to the root node.
  ──────
  ### Approach 3: Undo-Only Logging (In-Place Updates with Rollback Files)

  Instead of a full Redo/Undo WAL, a database can maintain only Undo Logs (also called rollback segments):

  1. Before modifying a page on disk, the database saves a copy of the original data (pre-image) into an Undo file.
  2. The dirty page can then be modified on disk.
  3. At Commit: The database must force all modified pages to disk (Force policy) and clear the undo records.
  4. On Crash / Abort: The recovery algorithm scans the Undo file and overwrites modified disk pages with their original pre-
  images to rollback uncommitted changes.

  #### Trade-offs:

  • 🟢 Allows uncommitted pages to be written to disk when RAM is full.
  • ❌ Requires synchronous disk writes at commit time (since there is no Redo log to re-apply committed data after a crash).
  ──────
  ### Approach 4: In-Memory DBs with Non-Volatile Memory (NVM / PMEM)

  Modern in-memory databases operating on Non-Volatile Dual In-line Memory Modules (NVDIMM / PMEM) bypass traditional disk
  block devices:

  • RAM itself is non-volatile (retains state across power loss).
  • Pointer operations and memory writes are directly persistent using CPU cache-flush instructions (clwb, sfence).
  • Atomicity is achieved using hardware-level atomic memory instructions (e.g., 64-bit atomic compare-and-swap) or
  transactional memory architectures.


Will go with No-steal force approach



For normal application as soon as insert is executed the data will be committed to the RAM and disk.
For Transaction we will add the data to the RAM then on commit we will push to disk.


for rollback  keep track of pages that are changed and only paste the
  changes from disk page to RAM 