if i am writing data per mili seconds then no-steal and force might be problamatic right? like it will be slower compared to WAL.

Yes, absolutely. If you are writing data every millisecond (~1,000 writes/second), a Force policy will become a severe bottleneck.

  Given that potDB is designed for recording time-series positions (Time → Position), millisecond-level ingestion is the exact scenario where a WAL (with No-Force)
  dramatically outperforms Force.
  Here is why Force will choke, and how WAL fixes it:
  ──────
  ### 1. Why "Force" Struggles with Millisecond Writes

  #### A. Hardware Limits (fsync / Disk Latency)

  • To guarantee durability in a Force policy, every write must flush to physical storage before returning:
      • NVMe SSD: A synchronous flush takes roughly 0.5 ms – 2 ms.
      • HDD / Slower disks: A synchronous flush takes 10 ms – 15 ms.
  • If each write takes ~1 ms just waiting for disk sync, your database will max out at 500–1,000 writes/sec with 100% disk saturation, causing latency spikes and dropped
  updates.

  #### B. Random I/O vs. Sequential I/O
  • In potDB, data is organized as a B-Tree across 4096-byte pages in pager.c.
  • Under Force, each insert seeks to arbitrary page offsets (lseek + write) to overwrite nodes in place. Random writes are significantly slower and harder on SSD write
  endurance than sequential writes.

  #### C. Massive Write Amplification
  • A single position update might only be ~32 bytes (timestamp + position).
  • In Force, updating 32 bytes forces an entire 4,096-byte page to be written to disk. If the insert causes a B-Tree split, it flushes 2–3 pages (8 KB – 12 KB written for 32
  bytes of payload).
  ──────
  ### 2. How WAL (No-Force) Solves This

  Switching to a No-Force + WAL model completely changes the write path:
    [Write / 1ms] ───> Append small record to WAL (Sequential I/O) ───> Return immediately
                              │
                       (In background)
                              ▼
                Checkpoint dirty B-Tree pages to disk
  1. Sequential Append Only:
  Writing to a WAL is purely an append to the end of the file. Sequential I/O is the fastest operation storage controllers and operating systems can perform.
  2. Deferred Page Flushes (No-Force):
  The actual B-Tree pages are updated in RAM only. The database does not write to the main .db file on every insert.
  3. Batching / Group Commits:
  If 5 writes arrive within 2 milliseconds, the OS or database can flush them in a single sequential I/O operation instead of 5 separate random page writes.
  4. Checkpointing:
  Flushing dirty B-Tree pages to disk is offloaded to a background checkpoint or done periodically (e.g., every 1,000 frames or every few seconds), completely decoupling
  commit latency from disk I/O.
  ──────
  ### Summary
   Metric                                                  | Current (No-Steal / Force)                              | With WAL (No-Force)
  ---------------------------------------------------------|---------------------------------------------------------|---------------------------------------------------------
   I/O Pattern                                             | Random page writes                                      | Fast sequential appends
   Write Amplification                                     | High (4 KB+ written per row)                            | Low (only the record/frame appended)
   Latency per Write                                       | Bound by disk write/flush time                          | In-memory speed + sequential log append
   Suitability for 1ms writes                              | ❌ Prone to bottlenecks and lag                         | Designed specifically for this

  If you plan to ingest data at millisecond intervals, implementing the WAL (using the log structures you started in wal.h and wal.c) is definitely the right architectural move.