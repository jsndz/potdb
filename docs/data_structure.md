When building a database strictly mapping **Timestamp $\to$ Position** for a **single physical entity**, the core engineering challenge is **dimensionality**.

Time is strictly 1-dimensional and moves forward. Space is 2-dimensional ($X, Y$) or 3-dimensional ($X, Y, Z$). Every data structure balances a trade-off between:

1. **Write efficiency** (appending fast streaming samples without write amplification).
2. **Temporal query speed** (*"Where was it from $T_1$ to $T_2$?"*).
3. **Spatial query speed** (*"Was it ever inside area $[X_1..X_2, Y_1..Y_2]$?"*).
4. **Memory / Storage footprint** (running on edge flash, RAM, or battery).

Here is a breakdown of every primary data structure family you can use, how they work for this exact problem, and their trade-offs.

---

### 1. Sequential & Log Structures (Time-Optimized)

These treat the database essentially as an append-only timeline.

#### A. Chunked Append-Only Array / Segment Log

* **How it works:** Incoming samples $(t, x, y)$ are buffered in memory and appended directly to fixed-size contiguous disk blocks (e.g., 64 KB to 4 MB). Each chunk header contains the start and end timestamp.
* **Write performance:** $O(1)$. Highest possible throughput; zero write amplification.
* **Temporal queries:** Binary search across chunk headers to find the starting block ($O(\log N)$), followed by a sequential scan or sub-block interpolation search.
* **Spatial queries:** Slow ($O(N)$). Requires scanning every single record unless paired with chunk-level bounding boxes.
* **Best used when:** 90%+ of queries are temporal tracks, replays, or time-slice visualizers.

#### B. Log-Structured Merge-Tree (LSM-tree)

* **How it works:** Writes buffer in an in-memory sorted structure (SkipList/MemTable) and an append-only WAL. When full, they flush to disk as immutable SSTables. Background threads periodically compact overlapping files.
* **Write performance:** Fast in-memory writes, but incurs continuous write amplification and disk I/O churn due to background compactions.
* **Temporal queries:** Fast if sorted by timestamp. The engine merges iterators across active MemTables and SSTable levels.
* **Spatial queries:** Inefficient. If sorted by time, finding coordinates in a bounding box requires scanning the whole time range.
* **Best used when:** Data arrives out-of-order (e.g., intermittent sensor connectivity) or updates/deletions are required.

---

### 2. Multi-Dimensional Tree Structures (Space-Optimized)

These treat points as coordinates in multi-dimensional space (either 2D $[X, Y]$ or 3D $[X, Y, T]$).

#### A. 3D R-Tree / $R^*$-Tree

* **How it works:** Groups nearby points into hierarchical Minimum Bounding Boxes (MBRs) in 3D space: $([X_{\min}, X_{\max}], [Y_{\min}, Y_{\max}], [T_{\min}, T_{\max}])$.
* **Write performance:** Slow ($O(\log N)$ to $O(N)$). Inserting points causes bounding boxes to expand, frequently triggering expensive node splits and tree rebalancing.
* **Temporal & Spatial queries:** Balanced. Excellent at answering combined queries like *"Find all points inside this bounding box between 2:00 PM and 2:15 PM"* because the tree prunes branches that don't intersect the 3D cube.
* **Best used when:** Static or batch-loaded datasets where reads heavily dominate writes. Poor fit for high-frequency live streaming.

#### B. $k\text{-d}$ Tree (k-Dimensional Tree)

* **How it works:** A binary search tree where each level alternates splitting axes (e.g., Level 1 splits by $T$, Level 2 by $X$, Level 3 by $Y$, Level 4 by $T$).
* **Write performance:** Dynamic updates require rebalancing, or the tree becomes skewed.
* **Temporal & Spatial queries:** Efficient nearest-neighbor and range searches.
* **Best used when:** Read-only historical segments where points can be bulk-loaded and balanced in memory once.

#### C. Octree (or Quadtree + Time Intervals)

* **How it works:** A Quadtree recursively subdivides 2D space into 4 quadrants; an Octree subdivides 3D space ($X, Y, T$) into 8 octants.
* **Write performance:** Better than R-trees if coordinate boundaries are known in advance, but still requires pointer manipulation and dynamic memory allocation per point.
* **Best used when:** Spatial bounds of the operating environment (e.g., a room or warehouse) are strictly bounded and fixed.

---

### 3. Space-Filling Curve Mappings (1D Flat Storage for Multi-Dimensional Data)

These algorithms mathematically fold 2D or 3D space into a single 1D scalar line, allowing you to use simple 1D sorted storage (B-trees, flat files, or LSMs) for multi-dimensional data.

```
       Spatial Grid (2D)                    1D Curve Order
   ┌─────┬─────┬─────┬─────┐
 3 │  5  │  7  │ 13  │ 15  │
   ├─────┼─────┼─────┼─────┤
 2 │  4  │  6  │ 12  │ 14  │          0 ──> 1 ──> 2 ──> 3
   ├─────┼─────┼─────┼─────┤                      │
 1 │  1  │  3  │  9  │ 11  │          4 ──> 5 ──> 6 ──> 7
   ├─────┼─────┼─────┼─────┤                      │
 0 │  0  │  2  │  8  │ 10  │          8 ──> 9 ──> 10 ──> 11
   └─────┴─────┴─────┴─────┘                      │
      0     1     2     3            12 ──> 13 ──> 14 ──> 15

```

#### A. Morton Code (Z-Order Curve)

* **How it works:** Takes the binary representations of $X$, $Y$, and $T$ and interleaves their bits:

$$\text{Key} = t_2 x_2 y_2 \, t_1 x_1 y_1 \, t_0 x_0 y_0$$


* **Write performance:** Very fast. Bit-shifting coordinates and timestamps into a 64-bit integer is a pure CPU register operation ($O(1)$).
* **Temporal & Spatial queries:** Maps a 2D/3D range query to a set of 1D continuous sub-ranges. However, Z-order curves suffer from sudden "jumps" across space boundaries, causing range queries to return non-relevant points that must be filtered out.

#### B. Hilbert Curve

* **How it works:** A continuous fractal space-filling curve that preserves spatial locality better than Z-order curves. Two points that are close along the 1D Hilbert index are guaranteed to be close in 2D/3D space.
* **Write performance:** Slower bit-calculation math than Morton codes, but still fast ($O(1)$).
* **Temporal & Spatial queries:** Tighter bounding-box checks with fewer false positives than Z-order curves.

#### C. Discrete Global Grid Systems (H3 / S2 / Geohash)

* **How it works:** Specifically designed for Earth GPS coordinates. Partitions the sphere into hierarchical hexagonal (H3) or quad (S2) cells, represented as 64-bit integers.
* **Write performance:** Single integer conversion per sample.
* **Queries:** Translates a geographic search radius into a list of cell IDs.

---

### 4. Hybrid / Inverted Structures (Two-Tier Design)

Rather than trying to force one data structure to handle both axes, hybrid approaches maintain two separate, synchronized representations.

#### A. Chunked Time Log + Spatial Grid Inverted Index

* **How it works:**
1. Points are written chronologically into flat, append-only files (the primary store).
2. A secondary lookup table maps spatial cells (e.g., an H3 index or grid cell) to an array of file offsets where the entity entered that cell:

$$\text{Cell ID} \longrightarrow [(\text{Chunk}_1, \text{Offset}_a), (\text{Chunk}_5, \text{Offset}_b), \dots]$$




* **Write performance:** Near-optimal. Primary writes are pure sequential appends. The spatial index only needs an update when the entity crosses from one grid cell into another.
* **Temporal queries:** $O(1)$ scan directly against primary chunks.
* **Spatial queries:** Read the inverted list for cell IDs matching the target zone, then seek directly to those chunk offsets.

#### B. Spatio-Temporal Interval Trees (Trajectory Line Segments)

* **How it works:** Instead of indexing every discrete point $(t, x, y)$, continuous motion between points is compressed into trajectory segments/vectors (e.g., line-simplification via Ramer-Douglas-Peucker). The segments are indexed using 1D Interval Trees along the time dimension and R-trees along the space dimension.
* **Write performance:** Medium. Requires buffering a window of points to perform trajectory compression before writing to disk.
* **Storage footprint:** Extremely small (often 90%+ reduction in total points stored).

---

### Comparison of Options

| Data Structure Family | Primary Strength | Critical Weakness | Best Fit Scenario |
| --- | --- | --- | --- |
| **Chunked Append-Only Log** | Maximum write speed, zero write amplification | Spatial queries require full scan | Pure chronological tracking, dashcams, flight recorders |
| **LSM-Tree** | Handles random writes and out-of-order samples | Background compaction burns I/O & battery; poor spatial locality | Multi-threaded ingest servers with network latency |
| **3D R-Tree** | Direct 3D bounding-box filtering ($X, Y, T$) | Expensive tree rebalancing; terrible streaming write throughput | Static analytical databases, offline trajectory analysis |
| **Space-Filling Curves (Z-order/Hilbert)** | Maps 2D/3D queries into standard 1D ordered stores | Range queries require post-filtering to remove boundary false positives | Key-Value stores where you want a single unified index |
| **Hybrid (Log + Inverted Grid)** | Fast writes + fast spatial pruning without heavy indexing | Dual-bookkeeping (must manage two separate files/tables) | Custom embedded databases for robots, vehicles, and wearables |



Approach is Chunked append only log + inverted spacial index
Since the data is mainly ordered and comes from a single entity( data is sorted automatically) we are not using LSM which is build for random access 