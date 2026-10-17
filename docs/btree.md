# B+ Tree Architecture & Memory Layout

**potDB** uses an on-disk, page-aligned B+ Tree to index spatial records chronologically.

---

## 1. Page-Aligned Node Representation

Every node in the B+ Tree occupies a single **4096-byte (4 KB) page** allocated and managed by the [`Pager`](../include/pager.h). Nodes are directly mapped in memory using byte offsets without any serialization or hidden struct padding overhead.

### Common Node Header (6 bytes)
Every node starts with a 6-byte header:

| Offset | Field | Type | Size | Description |
| :--- | :--- | :--- | :--- | :--- |
| `0` | `NODE_TYPE` | `uint8_t` | 1 byte | `NODE_INTERNAL` (`0`) or `NODE_LEAF` (`1`) |
| `1` | `IS_ROOT` | `uint8_t` | 1 byte | `1` if this page is the tree root, `0` otherwise |
| `2 - 5` | `PARENT_POINTER` | `uint32_t` | 4 bytes | Page number of the parent node |

---

## 2. Leaf Node Structure (`NODE_LEAF`)

Leaf nodes store the actual key-value pairs (`Row` records) and maintain pointers to adjacent sibling leaves for sequential range queries.

### Memory Layout

```
+------------------------------------------------------------------------------------+
|                                    HEADER (14 bytes)                               |
+---------------+---------------+------------------+-----------------+---------------+
| node_type     | is_root       | parent_pointer   | num_cells       | next_leaf     |
| (1 byte)      | (1 byte)      | (4 bytes)        | (4 bytes)       | (4 bytes)     |
| Offset 0      | Offset 1      | Offset 2         | Offset 6        | Offset 10     |
+---------------+---------------+------------------+-----------------+---------------+
|                                    CELLS BODY                                      |
+------------------------------------------------------------------------------------+
| Cell 0: Key (time_t, 8B) | Value (Row, 16B)                                        |
+------------------------------------------------------------------------------------+
| Cell 1: Key (time_t, 8B) | Value (Row, 16B)                                        |
+------------------------------------------------------------------------------------+
| ...                                                                                |
+------------------------------------------------------------------------------------+
| Cell N-1                                                                           |
+------------------------------------------------------------------------------------+
| Unused space (up to 4096 bytes)                                                    |
+------------------------------------------------------------------------------------+
```

### Specifications
- **Header Size**: 14 bytes (`COMMON_NODE_HEADER_SIZE` + `LEAF_NODE_NUM_CELLS_SIZE` + `LEAF_NODE_NEXT_LEAF_SIZE`).
- **Cell Size**: 24 bytes (`time_t` timestamp [8B] + `Row` [16B: 8B timestamp + 8B `Position { int x, y }`]).
- **Max Cells**: $\lfloor (4096 - 14) / 24 \rfloor = \mathbf{170\text{ cells}}$.
- **Min Cells (Underflow Threshold)**: $\mathbf{85\text{ cells}}$ (`LEAF_NODE_MIN_CELLS = (LEAF_NODE_MAX_CELLS + 1) / 2`).

---

## 3. Internal Node Structure (`NODE_INTERNAL`)

Internal nodes route searches down the tree hierarchy toward the appropriate leaf page.

### Memory Layout

```
+------------------------------------------------------------------------------------+
|                                    HEADER (14 bytes)                               |
+---------------+---------------+------------------+-----------------+---------------+
| node_type     | is_root       | parent_pointer   | num_keys        | right_child   |
| (1 byte)      | (1 byte)      | (4 bytes)        | (4 bytes)       | (4 bytes)     |
| Offset 0      | Offset 1      | Offset 2         | Offset 6        | Offset 10     |
+---------------+---------------+------------------+-----------------+---------------+
|                                    CELLS BODY                                      |
+------------------------------------------------------------------------------------+
| Cell 0: child_page_num (uint32_t, 4B) | key (time_t, 8B)                           |
+------------------------------------------------------------------------------------+
| Cell 1: child_page_num (uint32_t, 4B) | key (time_t, 8B)                           |
+------------------------------------------------------------------------------------+
| ...                                                                                |
+------------------------------------------------------------------------------------+
| Unused space (up to 4096 bytes)                                                    |
+------------------------------------------------------------------------------------+
```

### Specifications
- **Header Size**: 14 bytes (`COMMON_NODE_HEADER_SIZE` + `INTERNAL_NODE_NUM_KEYS_SIZE` + `INTERNAL_NODE_RIGHT_CHILD_SIZE`).
- **Cell Size**: 12 bytes (`child_page_num` [4B] + `key` [8B]).
- **Routing Invariant**:
  - `Cell[i].child` holds keys $\le \text{Cell}[i].key$.
  - `right_child` holds keys $> \text{Cell}[\text{num\_keys}-1].key$.
- **Max Keys**: **3 keys** (`INTERNAL_NODE_MAX_KEYS = 3`), pointing to up to 4 child nodes.
- **Min Keys (Underflow Threshold)**: **1 key** (`INTERNAL_NODE_MIN_KEYS = 1`).

---

## 4. Search Algorithm

1. Begin at `table->root_page_num` (Page `0`).
2. If the current page is an **Internal Node**:
   - Perform binary search over `internal_node_key(node, i)`.
   - If `key <= internal_node_key(node, i)`, descend into `internal_node_child(node, i)`.
   - If `key > all keys`, descend into `internal_node_right_child(node)`.
3. If the current page is a **Leaf Node**:
   - Perform binary search over `leaf_node_key(node, i)` to find the exact cell index.
   - Return a `Cursor` pointing to `(page_num, cell_num)`.

---

## 5. Insert & Split Algorithm

1. Find the target leaf node via `table_find(table, key)`.
2. Check for duplicate keys. If a duplicate exists, return `EXECUTE_DUPLICATE_KEY`.
3. If the leaf has space (`num_cells < 170`), shift cells right to maintain sorted order and insert the new cell.
4. If the leaf is full (`num_cells == 170`):
   - Allocate a new page via `get_unused_pages(pager)`.
   - Split cells: 86 cells remain in the left node, 85 cells move to the right node.
   - Update leaf sibling linked list: `left->next_leaf = new_page`.
   - If the splitting node was the **root**, invoke `create_new_root_node` to allocate a new internal root and increase tree height.
   - If non-root, propagate the new child and routing key to the parent internal node using `internal_node_insert` (recursively splitting internal nodes if `num_keys >= 3`).

---

## 6. Deletion & Rebalancing Algorithm

Deletion maintains tree balance through borrowing (redistribution) and merging (coalescing).

```
                      leaf_node_delete(table, key)
                                   │
                      Locate cell via table_find()
                                   │
                     Shift cells left & decrement
                                   │
                    Did max key of leaf change?
                     ├─ Yes ──> update_ancestor_keys()
                     └─ No  ──┐
                              │
                    Check leaf underflow:
                  num_cells < LEAF_NODE_MIN_CELLS?
                     ├─ No  ──> Done
                     └─ Yes ──> leaf_node_handle_underflow()
                                   │
             ┌─────────────────────┴─────────────────────┐
             ▼                                           ▼
  Can borrow from Sibling?                    Both siblings at min?
  (Left or Right > MIN)                                  │
             │                                    Merge with Sibling:
   Shift & transfer 1 cell                     - Append all cells
   Update routing key in parent                - Update next_leaf pointer
             │                                 - internal_node_remove_child()
           Done                                          │
                                                Check parent underflow:
                                              num_keys < MIN_KEYS?
                                              (Cascade recursively)
```

### Steps in Detail

1. **Locate & Remove**:
   - Locate the cell index via binary search.
   - Remove the cell by shifting all subsequent cells left by 24 bytes and decrementing `num_cells`.
2. **Ancestor Routing Key Updates**:
   - If the deleted cell was the node's maximum key, traverse up parent nodes using `update_ancestor_keys`.
   - If the node is an interior child in its parent, update `internal_node_key(parent, index)`.
   - If the node is the `right_child` of its parent, the parent's own max key changed; recurse upward.
3. **Leaf Underflow (`num_cells < 85`)**:
   - **Root Leaf**: Allowed to hold anywhere from 0 to 170 cells; no rebalancing needed.
   - **Borrow from Left Sibling**: If left sibling has $> 85$ cells, transfer its largest cell to index 0 of the current node and update the parent routing key.
   - **Borrow from Right Sibling**: If right sibling has $> 85$ cells, transfer its smallest cell to the end of the current node and update routing keys.
   - **Merge with Sibling**: If neither sibling can lend:
     - Coalesce with the left sibling (or right sibling if at the left boundary).
     - Update the singly linked list pointer `leaf_node_next_leaf` to bypass the merged-away node.
     - Call `internal_node_remove_child` on the parent to remove the vacated child page.
4. **Internal Node Cascading & Tree Shrinking**:
   - When a child is removed from an internal node, remaining cells shift left.
   - If a non-root internal node drops below `INTERNAL_NODE_MIN_KEYS` (0 keys), `internal_node_handle_underflow` redistributes or merges internal nodes.
   - If the **root** internal node key count drops to 0, its only remaining child is copied directly into Page 0, shrinking the overall tree height by 1.
