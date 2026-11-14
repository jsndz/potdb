# Contributing to potDB

Thank you for your interest in contributing to **potDB**! potDB is an embedded, append-only spatiotemporal time-series database designed for lightweight spatial telemetry.

Whether you are reporting a bug, improving documentation, optimizing query traversal, or proposing new features, your help is welcome.

---

## Code of Conduct

This project adheres to the [Contributor Covenant Code of Conduct](CODE_OF_CONDUCT.md). By participating, you are expected to uphold this code.

---

## Getting Started

### Prerequisites

potDB is written in standard POSIX C99 and requires:
- A C compiler supporting C99 (`gcc` or `clang`)
- POSIX-compliant operating system (Linux, macOS, FreeBSD, etc.)
- GNU `make`
- *(Optional)* `valgrind` for memory leak and address sanitization checks
- *(Optional)* Emscripten (`emcc`) for WebAssembly builds

### Cloning and Building

```bash
# Clone the repository
git clone https://github.com/jaisondz/potdb.git
cd potdb

# Build the standalone binary and static library
make

# Clean build artifacts
make clean
```

---

## Development & Testing Workflow

All code contributions must pass the complete test suite without compiler warnings.

### Running Tests

```bash
# Run all test suites (unit, edge-case, and high-volume stress)
make all_tests

# Or run individual suites
make test    # Core functionality & durability modes
make edge    # Boundary conditions, malformed/corrupted files
make stress  # 1M points round-trip and multi-session continuity
```

### Running Benchmarks

```bash
# Compile and execute the benchmark suite
make bench
```

### Memory Verification

Before submitting changes that touch memory buffers or file I/O, verify that there are no leaks or invalid memory accesses:

```bash
valgrind --leak-check=full --show-leak-kinds=all --error-exitcode=1 ./build/test_unit
valgrind --leak-check=full --show-leak-kinds=all --error-exitcode=1 ./build/test_edge
```

---

## Coding Standards & Guidelines

1. **C99 Standard**: Code should adhere to `-std=c99 -Wall -Wextra -D_GNU_SOURCE`. Zero compiler warnings allowed.
2. **Explicit Endianness & Packing**:
   - potDB uses explicit little-endian serialization (`put_le16`, `put_le32`, `put_le64`, `put_le_float`). Never use raw `memcpy` of multi-byte numbers across platform boundaries without considering endianness.
   - On-disk headers use `#pragma pack(push, 1)`. Avoid unaligned pointer dereferencing on packed memory structures.
3. **Memory Management**:
   - The write path allocates memory once during `ptdb_open` and must perform zero dynamic allocations (`malloc`/`free`) per coordinate insertion.
   - Ensure every allocated resource (`fd`, buffer pointers) is cleanly released in error and close paths.
4. **POSIX Compliance**:
   - Keep the codebase self-contained without third-party C library dependencies.
   - Use standard POSIX calls (`open`, `close`, `read`, `write`, `lseek`, `ftruncate`, `fsync`). Handle `EINTR` and partial writes using `ptdb_write_all`.

---

## Pull Request Process

1. **Fork and Branch**:
   - Fork the repository on GitHub.
   - Create a feature or bugfix branch from `master` (`git checkout -b feat/your-feature-name` or `fix/your-bugfix-name`).

2. **Commit Messages**:
   - Use concise, descriptive conventional commit messages:
     - `feat: add sparse in-memory index for binary time search`
     - `fix: handle partial chunk header write on disk full`
     - `docs: update integration guide for CMake`
     - `test: add fuzzing for corrupted chunk magic bytes`

3. **Validation**:
   - Ensure `make all_tests` passes cleanly.
   - If adding new features, include corresponding unit or edge-case tests in `tests/`.
   - Update relevant documentation in `docs/` or `README.md`.

4. **Submit**:
   - Push your branch to GitHub and open a Pull Request against `master`.
   - Fill in the PR template details.
