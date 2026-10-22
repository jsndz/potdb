Initial idea for rollback was to do logical row deletion but it is bad

  Here is why:
  ──────
  ### 1. Logical Deletion Cannot Undo B-Tree Page Splits

  This is the most critical issue:

  • When you insert rows during a transaction, a node might fill up and split into two pages (leaf_node_split_and_insert()). This allocates a new page, updates parent internal nodes, and
  may even create a new root.
  • If you run logical leaf_node_delete() to undo the inserts:
      • Deleting a key removes the cell, but it does not cleanly invert the page split.
      • B-tree underflow merges and rebalancing rules will rarely reconstruct the exact original page layout.
      • Newly allocated pages linger or cause fragmentation instead of disappearing.
  • Page Reloading + pager_truncate() guarantees the B-tree returns to the exact byte-for-byte physical structure it had before the transaction began.
  ──────
  ### 2. Performance: O(pages) vs O(N log M) Tree Operations

  Imagine a transaction that inserts 500 rows across 3 leaf pages:

   Method                | Work Required                                                                                                | Complexity
  -----------------------|--------------------------------------------------------------------------------------------------------------|----------------------------------------------------
   Logical Deletion      | 500 B-tree lookups (table_find), 500 cell shifts (memcpy), and up to 500 underflow rebalance checks across   | O(N log M) heavy CPU & tree traversal
                         | parent nodes.                                                                                                |
   Page Reload / Restore | Simply re-read the 3 modified pages (or restore them from an in-memory pre-image buffer) and truncate.       | O(pages modified) fast sequential memory/disk copy
  ──────
  ### 3. Handles Updates and Overwrites Automatically

  • If a transaction updates an existing row from value A → B:
      • Logical deletion would completely delete the row from the database instead of restoring it back to A (unless you design an entire record-level undo-log mechanism).
      • Page reloading automatically restores A because the clean page on disk still has the original value A.

  ──────
  ### 4. Rollback Must Never Fail

  • Logical deletion involves complex branching logic: parent key updates, sibling borrowing, node merging, and underflow handling (leaf_node_handle_underflow()). If an edge-case bug occurs
  during deletion, the database is left in a corrupted state midway through an abort.
  • Physical page reload is just copying clean bytes. It is deterministic, isolated, and practically impossible to fail.


The better approach is   restoring/reloading pages from db

But it there are better approaches than this 



solutions 
1. In-memory shadow pages
When transaction is about to modify a page make a copy 
 do changes in main  page 
on commit just delete the copy
on rollback do memcpy the copy into main

Adv: fast 
Dis: Keeping copy of thousand maybe too much


2. COW / Shadow paging b-tree

on change
create a new copy page 
duplicate till ancestor then root
Keep old root pointer
on commit replace the root
on rollback discatd the copy page 