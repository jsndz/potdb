# potDB Integration & Embedding Guide

This guide explains how to embed `potDB` (PTDB) into any C, C++, or systems application as an embedded, append-only time-series database.

---

## 1. Quickstart (60 Seconds)

### Step 1: Build the Static Library
Run `make lib` in the `potDB` repository:
```bash
make lib
# Generates: build/libptdb.a
```

### Step 2: Include and Link
Include [`include/ptdb.h`](../include/ptdb.h) in your C/C++ source code:

```c
#include "ptdb.h"
#include <stdio.h>

int main(void)
{
    // 1. Initialize file (dim: 2D, time_unit: ms)
    ptdb_init("telemetry.db", 2, PTDB_TIME_UNIT_MS);

    // 2. Open for writing with periodic syncing
    PTDBConfig config = {
        .max_points = 1000,
        .max_time_window_ms = 5000,
        .sync_mode = PTDB_SYNC_PERIODIC,
        .sync_interval_flushes = 10
    };
    PTDB *db = ptdb_open("telemetry.db", &config);

    // 3. Append spatial coordinates
    ptdb_insert_2d(db, 1700000000, 12.34f, 56.78f);
    ptdb_insert_2d(db, 1700000010, 12.50f, 56.90f);

    // 4. Close (commits remaining points and performs final fsync)
    ptdb_close(db);
    return 0;
}
```

### Step 3: Compile Your Application
```bash
gcc app.c -I/path/to/potdb/include -L/path/to/potdb/build -lptdb -o app
```

---

## 2. Integration Patterns

### Pattern A: Static Library Linking (Recommended)
Compile `libptdb.a` and link it directly against your executable:
```makefile
CFLAGS += -I$(POTDB_DIR)/include
LDFLAGS += -L$(POTDB_DIR)/build -lptdb
```

### Pattern B: CMake Integration
If your project uses CMake, you can add `potDB` as a static library target:
```cmake
# Add potDB include directory
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/potdb/include)

# Option 1: Link prebuilt libptdb.a
add_executable(my_app main.c)
target_link_libraries(my_app ${CMAKE_CURRENT_SOURCE_DIR}/potdb/build/libptdb.a)

# Option 2: Add potDB sources directly into your build
add_library(ptdb STATIC
    potdb/src/crc32.c
    potdb/src/writer.c
    potdb/src/reader.c
)
target_include_directories(ptdb PUBLIC potdb/include)
target_link_libraries(my_app ptdb)
```

### Pattern C: Git Submodule
Add `potDB` as a submodule in your repository:
```bash
git submodule add https://github.com/jsndz/dbaCe.git external/potdb
cd external/potdb && make lib
```

---

## 3. Core API Workflows

### A. Writing Spatial Coordinates (2D & 3D)

```c
// 2D Write: (timestamp, x, y)
int ptdb_insert_2d(PTDB *db, int64_t timestamp, float x, float y);

// 3D Write: (timestamp, x, y, z)
int ptdb_insert_3d(PTDB *db, int64_t timestamp, float x, float y, float z);

// Generic struct write
Point2D p2 = { .timestamp = 1700000000, .x = 10.0f, .y = 20.0f };
ptdb_insert_point(db, &p2);
```
> **Error Codes**:
> - Returns `0` on success.
> - Returns `-1` on schema mismatch (e.g. inserting 3D point into 2D database).
> - Returns `-2` on non-monotonic timestamp ($t_{\text{new}} < t_{\text{last}}$).

---

### B. Two-Level Spatiotemporal Range Queries

Querying points by time and bounding box executes in two stages:
1. **Level 1 (Chunk Skipping)**: Uses the chunk header's time window and MBR bounding box to skip non-matching chunks using `lseek()`, bypassing disk payload reads.
2. **Level 2 (Point Filtering)**: Scans points within intersecting chunks and invokes a user-defined callback function for exact matches.

```c
static void on_point_match(const Point2D *point, void *user_data)
{
    printf("Matched: t=%ld, pos=(%.2f, %.2f)\n", (long)point->timestamp, point->x, point->y);
}

// Define the spatiotemporal bounding box
PTDBQueryRange2D range = {
    .min_time = 1700000000,
    .max_time = 1700003600,  // 1-hour window
    .min_x = 10.0f, .max_x = 50.0f,
    .min_y = -20.0f, .max_y = 20.0f
};

uint64_t total_matched = ptdb_query_range_2d("telemetry.db", &range, on_point_match, NULL);
```

---

### C. Sequential Chunk Traversal (`PTDBReader`)

For analytics, bulk export, or offline replay:

```c
PTDBReader *reader = ptdb_reader_open("telemetry.db");
ChunkHeader2D ch;
void *points = NULL;
uint32_t count = 0;

while (ptdb_reader_next_chunk(reader, &ch, &points, &count) == 1) {
    // ch.checksum is automatically verified with CRC-32
    // ch.min_x, max_x, min_y, max_y represent chunk MBR bounds
    for (uint32_t i = 0; i < count; i++) {
        Point2D pt;
        ptdb_get_point_2d(points, i, &pt);
        // Process point pt.timestamp, pt.x, pt.y
    }
}
ptdb_reader_close(reader);
```

---

## 4. Configuration & Tuning Recipes

Tune `PTDBConfig` according to your application profile:

### Recipe 1: Drone / Autonomous Vehicle Telemetry (High Frequency)
High write volume where small loss windows are acceptable on hardware power loss:
```c
PTDBConfig config = {
    .max_points = 1000,
    .max_time_window_ms = 1000,
    .sync_mode = PTDB_SYNC_PERIODIC,
    .sync_interval_flushes = 10   // fsync once every 10,000 points
};
```

### Recipe 2: Critical Financial / Audit Event Logs (Crash Proof)
Guarantee physical flash persistence for every single chunk:
```c
PTDBConfig config = {
    .max_points = 500,
    .max_time_window_ms = 5000,
    .sync_mode = PTDB_SYNC_ALWAYS  // fsync on every chunk flush
};
```

### Recipe 3: Bulk Batch Import / Dataset Ingestion (Maximum Throughput)
Bypass `fsync()` and let the OS Page Cache schedule writeback:
```c
PTDBConfig config = {
    .max_points = 50000,
    .max_time_window_ms = 60000,
    .sync_mode = PTDB_SYNC_NONE    // Reaches up to 5.4 Million points/sec
};
```

---

## 5. Concurrency & Thread-Safety Rules

1. **Single Writer**: A single `PTDB *` handle must not be written to concurrently by multiple threads without external synchronization (e.g. a `pthread_mutex_t`).
2. **Concurrent Readers**: Multiple readers can open and query the database concurrently using separate `PTDBReader *` handles.
3. **Writer + Reader Concurrency**: Because PTDB is strictly append-only and writes atomic chunks with valid headers and checksums, readers can query flushed chunks while the writer appends new chunks.
