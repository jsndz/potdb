# Changelog

All notable changes to **potDB** will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

---

## [Unreleased]

### Added
- Directory metadata durability (`sync_parent_directory`) on `ptdb_init`.
- Automatic crash recovery and torn chunk repair (`ftruncate`) during `ptdb_open`.
- Incremental streaming CRC32 calculation (`ptdb_crc32_update`) covering both chunk header metadata and payload.
- Cooperative time-based flush polling (`ptdb_poll`) for idle telemetry streams.
- Comprehensive edge-case validation suite and high-volume stress tests.

---

## [1.0.0] - 2026-11-03

### Added
- Core sequential append-only chunk storage architecture.
- 2D (`timestamp, x, y`) and 3D (`timestamp, x, y, z`) spatial telemetry support.
- Two-level spatiotemporal range queries with chunk header skipping (`ptdb_query_range_2d`, `ptdb_query_range_3d`).
- Three configurable durability modes:
  - `PTDB_SYNC_ALWAYS`: synchronous `fsync` on every chunk flush.
  - `PTDB_SYNC_PERIODIC`: amortized `fsync` every $N$ chunk flushes.
  - `PTDB_SYNC_NONE`: pure OS page cache delegation.
- Little-endian byte serialization with `#pragma pack(push, 1)` headers.
- Sequential chunk reader iterator (`PTDBReader`).
- Benchmark harness (`bench/bench_ptdb.c`) and historical performance tracking.
- WebAssembly (Wasm) compilation support via Emscripten.
