<div align="center">
  <img src="logo.svg" alt="potDB Logo" width="100" height="100" />
  <h1>potDB (PTDB)</h1>
  <p><strong>Lightweight, Append-Only On-Disk Spatial Time-Series Database in Pure C99</strong></p>
  <p>
    <a href="https://github.com/jaisondz/potdb/actions"><img src="https://img.shields.io/badge/CI-Passing-brightgreen.svg" alt="CI Status" /></a>
    <a href="LICENSE"><img src="https://img.shields.io/badge/License-MIT-blue.svg" alt="License: MIT" /></a>
    <img src="https://img.shields.io/badge/Standard-C99-orange.svg" alt="Standard: C99" />
    <img src="https://img.shields.io/badge/Dependencies-Zero%20(POSIX)-success.svg" alt="Zero Dependencies" />
  </p>
</div>

**potDB** is a lightweight, append-only spatial time-series storage engine written in standard POSIX C99. It is designed for recording, streaming, and querying high-frequency 2D and 3D telemetry:

```text
timestamp -> (x, y)  or  (x, y, z)
```

---

## Key Features

- **Chunked Append-Only Architecture**: Coordinates accumulate in an in-memory buffer and flush sequentially as immutable chunks. No in-place overwrites, eliminating lock contention and file fragmentation.
- **Strict Byte-Level Serialization**: Little-endian encoders and decoders prevent unaligned memory faults (`SIGBUS`) and ensure cross-platform portability across ARM and x86.
- **Configurable Durability Modes**:
  - `PTDB_SYNC_ALWAYS` *(Default)*: Calls `fsync()` on every chunk flush for maximum durability.
  - `PTDB_SYNC_PERIODIC`: Amortizes `fsync()` every $N$ flushes (e.g., every 10 or 20) to balance throughput with crash resilience.
  - `PTDB_SYNC_NONE`: Bypasses `fsync()`, delegating writeback to the OS Page Cache for extreme ingestion rates (up to **5.4M points/sec**).
- **Two-Level Spatiotemporal Range Queries**: Combines time windows and spatial bounding boxes (MBR). Non-overlapping chunks are skipped via `lseek()` at the header level before reading coordinate payloads.
- **Dual Flush Triggers**:
  - **Capacity Trigger**: Flushes immediately when the buffer reaches `max_points` (default: 500).
  - **Time-Window Trigger**: Flushes when `(t_current - t_start) >= max_time_window_ms` (default: 5,000 ms), preventing slow streams from getting stuck in RAM.
- **Data Integrity & Crash Recovery**: Every chunk is verified with an IEEE 802.3 CRC-32 checksum. Torn or incomplete writes caused by power failure are automatically detected and rolled back via `ftruncate()` upon restart.
- **Zero Third-Party Dependencies**: Self-contained POSIX C with no external libraries or background server processes to manage.

---

## Quickstart

### Build & Test

```bash
# Build the CLI binary (potdb) and static library (libptdb.a)
make

# Run the test suite (unit, edge-case, and stress tests)
make test
make all_tests

# Clean build artifacts
make clean
```

---

## CLI Usage

### 1. Initialize a Database
```bash
# Initialize a 2D database (millisecond precision)
./potdb positions_2d.db init 2

# Initialize a 3D database
./potdb positions_3d.db init 3
```

### 2. Insert Points
```bash
# Insert a 2D point: insert <timestamp> <x> <y>
./potdb positions_2d.db insert 1700000000 12.34 56.78

# Stream points from stdin
cat << 'EOF' | ./potdb positions_2d.db
1700000000 10.0 20.0
1700000010 10.5 20.5
1700000020 11.0 21.0
EOF
```

### 3. Query Across Time and Space
```bash
# Query: query <min_time> <max_time> <min_x> <max_x> <min_y> <max_y>
./potdb positions_2d.db query 1700000000 1700003600 10.0 50.0 -20.0 20.0
```

---

## C API Quick Example

Include [`include/ptdb.h`](include/ptdb.h) and link `build/libptdb.a`:

```c
#include "ptdb.h"

int main(void) {
    // 1. Initialize file schema
    ptdb_init("telemetry.db", 2, PTDB_TIME_UNIT_MS);

    // 2. Open writer with configuration
    PTDBConfig config = {
        .max_points = 500,
        .max_time_window_ms = 5000,
        .sync_mode = PTDB_SYNC_ALWAYS
    };
    PTDB *db = ptdb_open("telemetry.db", &config);

    // 3. Ingest points
    ptdb_insert_2d(db, 1700000000, 12.34f, 56.78f);
    ptdb_insert_2d(db, 1700000001, 12.35f, 56.79f);

    // 4. Close database (flushes remaining buffer and syncs)
    ptdb_close(db);

    return 0;
}
```

---

## Performance Highlights

*Benchmarks executed on AMD Ryzen 7 7840HS, NVMe PCIe Gen 4 SSD, Linux (Kernel 7.2):*

| Chunk Buffer (`max_points`) | `SYNC_ALWAYS` (`fsync`) | `SYNC_PERIODIC` ($N=10$) | `SYNC_NONE` (Page Cache) |
| :--- | :--- | :--- | :--- |
| **500** *(Default)* | 96,668 pts/sec | 813,048 pts/sec | **4,365,202 pts/sec** |
| **1,000** | 188,119 pts/sec | 1,385,184 pts/sec | **4,611,279 pts/sec** |
| **5,000** | 774,522 pts/sec | 3,252,558 pts/sec | **5,492,982 pts/sec** |

- **Sequential Scan & Validation**: **17.2 Million points/sec** (263 MB/sec) with full on-the-fly CRC-32 verification.
- **In-Memory Accessor Latency**: **~10.7 ns/point** for 2D points.

---

## Documentation

- [**Integration Guide**](docs/integration_guide.md): Handbook for embedding potDB via C, CMake, and FFI.
- [**Durability & Page Cache**](docs/auto_flush.md): Deep-dive into Linux Page Cache, `fsync()`, and durability trade-offs.
- [**WebAssembly Guide**](docs/wasm_compilation_guide.md): Compiling potDB for client-side web applications.
- [**Benchmark Report**](docs/benchmark_report.md): Detailed benchmark methodology and memory access analysis.

---

## Community & Contributing

Contributions, bug reports, and suggestions are welcome!

- **Contributing Guide**: See [**`CONTRIBUTING.md`**](CONTRIBUTING.md) for build, testing, and PR guidelines.
- **Code of Conduct**: This project is governed by the [**`CODE_OF_CONDUCT.md`**](CODE_OF_CONDUCT.md).
- **Security Policy**: For vulnerability reporting, see [**`SECURITY.md`**](SECURITY.md).
- **Changelog**: Release history is documented in [**`CHANGELOG.md`**](CHANGELOG.md).

---

## License

potDB is open-source software licensed under the **[MIT License](LICENSE)**.  
Copyright (c) 2026 Jaison.
