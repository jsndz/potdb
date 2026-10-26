# Binary File Formats: `#pragma pack(1)` vs. Explicit Byte Serialization

This document explores the architectural trade-offs, hardware implications, and systems engineering hazards of directly dumping `#pragma pack(1)` structs to disk versus explicit byte-level serialization in binary storage engines.
Consider the struct without `#pragma pack`: padding is added between struct variables to ensure that fields are properly aligned, making memory access efficient and safe. CPUs can access individual bytes, but aligned access to multi-byte values is often more efficient.
If you add padding, it can waste space in the file.
If you remove padding using `#pragma pack(1)`, it can cause unaligned memory access, which has performance costs and other hazards on some architectures.
So basically, the solution is to handle file access directly using byte-level conversion functions that serialize and deserialize struct fields, rather than writing or reading the struct directly.
---

## 1. Executive Summary

When developing high-throughput time-series databases and binary log engines in C, persisting structured records to disk typically follows one of two patterns:

1. **Direct Memory Mapping / Struct Dumping (`write(fd, &struct, sizeof(struct))`)**:
   Uses compiler directives such as `#pragma pack(push, 1)` to eliminate compiler-injected padding bytes, then writes the in-memory struct directly to persistent storage.
2. **Explicit Byte-Level Serialization (`put_le*` / `get_le*`)**:
   Iterates through struct fields individually, packing and unpacking them byte-by-byte into a byte buffer using well-defined endianness (e.g., Little-Endian).

While `#pragma pack(1)` is attractive for rapid prototyping and zero-copy in-memory manipulation, it introduces significant risks regarding **hardware alignment traps**, **compiler undefined behavior**, and **cross-architecture endianness incompatibility**.

---

## 2. The Three Architectural Hazards of `#pragma pack(1)`

### Hazard 1: Unaligned Memory Accesses & Hardware Faults (`SIGBUS`)

Modern CPU architectures (ARM, RISC-V, SPARC, MIPS, and even x86) access physical RAM in memory words (typically 32-bit or 64-bit boundaries). A naturally aligned memory access occurs when an $N$-byte value resides at a memory address that is a multiple of $N$:

- 2-byte integer (`uint16_t`): Address must be divisible by 2 (`addr % 2 == 0`).
- 4-byte integer / float (`uint32_t`, `float`): Address must be divisible by 4 (`addr % 4 == 0`).
- 8-byte integer / double (`uint64_t`, `int64_t`): Address must be divisible by 8 (`addr % 8 == 0`).

#### What `#pragma pack(1)` Does

When `#pragma pack(1)` is enabled, the compiler suppresses natural alignment padding. As a result, 64-bit integers and 32-bit floats can land on odd or unaligned byte boundaries.

#### Concrete Example in `ChunkHeader2D`:

```c
#pragma pack(push, 1)
typedef struct {
    uint16_t magic;         // Offset 0  (2 bytes)
    uint16_t count;         // Offset 2  (2 bytes)
    int64_t  start_time;    // Offset 4  (8 bytes) -> 4 IS NOT DIVISIBLE BY 8!
    int64_t  end_time;      // Offset 12 (8 bytes) -> 12 IS NOT DIVISIBLE BY 8!
    float    min_x, max_x;  // Offset 20, 24 (4 bytes each)
    float    min_y, max_y;  // Offset 28, 32 (4 bytes each)
    uint32_t payload_size;  // Offset 36 (4 bytes)
    uint32_t checksum;      // Offset 40 (4 bytes)
} ChunkHeader2D;            // Total: 44 bytes
#pragma pack(pop)
```

In this struct, `start_time` sits at byte offset `4`, and `end_time` sits at offset `12`. Neither offset is a multiple of 8.

#### Concrete Example in `Point3D`:

```c
#pragma pack(push, 1)
typedef struct {
    int64_t timestamp;  // Offset 0 (8 bytes)
    float x, y, z;      // Offset 8, 12, 16 (4 bytes each)
} Point3D;              // Total size: 20 bytes (20 is not divisible by 8)
#pragma pack(pop)
```

In an array `Point3D pts[]`:

- `pts[0]` starts at byte offset `0` (`timestamp` is 8-byte aligned).
- `pts[1]` starts at byte offset `20` (`20 % 8 != 0`). Its `timestamp` is **unaligned**!
- Every odd-indexed element in the array suffers from unaligned 64-bit timestamp accesses.

#### Consequences Across Architectures:

- **x86 / x86-64**: Modern Intel/AMD CPUs support unaligned memory access in hardware. However, if an access straddles two separate 64-byte CPU cache lines, an expensive **split-lock / cache-line split** occurs, degrading throughput.
- **ARMv7 / ARMv8 / ARM64**: Standard `LDR`/`STR` instructions may handle unaligned access, but paired load/store instructions (`LDP`/`STP`), atomic operations, and device memory accesses **immediately fault with `SIGBUS` (Bus Error)**.
- **Embedded & RISC-V / SPARC**: Hardware traps to kernel-space emulation (100x slowdown) or immediately crashes the process.

---

### Hazard 2: Host Endianness Coupling (Non-Portable Storage)

Direct struct writing with `write(fd, &header, sizeof(header))` dumps the raw bytes in whatever byte order the host CPU natively uses:

- **Little-Endian (LE)**: Intel/AMD x86-64, Apple Silicon (ARM64), Raspberry Pi.
- **Big-Endian (BE)**: Network equipment, IBM s390x mainframes, older SPARC/PowerPC systems.

If a database file written on a Little-Endian workstation is transferred to a Big-Endian system (or read by a service expecting network byte order):

```text
Host Value (uint32_t): 0x54534442
Stored on Little-Endian Disk:  [ 0x42, 0x44, 0x53, 0x54 ]
Read directly on Big-Endian:   0x42445354 (COMPLETELY CORRUPTED VALUE!)
```

A robust database file format must define an invariant on-disk endianness (typically Little-Endian for modern storage engines, or Big-Endian/Network Order). Direct struct writes bypass endianness normalization entirely.

---

### Hazard 3: Compiler Undefined Behavior & `-Waddress-of-packed-member`

According to the ISO C standard (§6.3.2.3), converting a pointer to a type with stricter alignment requirements than the target address leads to **Undefined Behavior (UB)**.

When using `#pragma pack(1)`, GCC and Clang will emit warnings when passing pointers to packed members:

```c
// Compilation Warning:
// taking address of packed member of 'struct ChunkHeader2D' may result in an unaligned pointer value
update_timestamp(&header->start_time);
```

Furthermore, when compilers optimize loops or vectorization (`-O2` / `-O3`), they may substitute scalar operations with SIMD instructions (e.g., SSE/AVX `movaps` / `movdqa` on x86, or NEON on ARM). These SIMD instructions strictly enforce 16-byte or 32-byte alignment; encountering an unaligned packed address results in an immediate segmentation fault (`SIGSEGV`).

---

## 3. Engineering Solutions

### Solution A: Explicit Byte-Level Serialization (The Gold Standard)

Used by production databases and binary protocols such as **SQLite**, **RocksDB**, **Protocol Buffers**, **Apache Kafka**, and **FlatBuffers**.

Instead of writing structs directly, values are packed and unpacked using bit-shifts or byte-by-byte copies into an intermediate contiguous byte array.

#### 1. Endian-Safe, Alignment-Safe Primitives:

```c
#include <stdint.h>
#include <string.h>

/* Write integer values in Little-Endian byte order */
static inline void put_le16(uint8_t *dst, uint16_t val) {
    dst[0] = (uint8_t)(val);
    dst[1] = (uint8_t)(val >> 8);
}

static inline void put_le32(uint8_t *dst, uint32_t val) {
    dst[0] = (uint8_t)(val);
    dst[1] = (uint8_t)(val >> 8);
    dst[2] = (uint8_t)(val >> 16);
    dst[3] = (uint8_t)(val >> 24);
}

static inline void put_le64(uint8_t *dst, uint64_t val) {
    dst[0] = (uint8_t)(val);
    dst[1] = (uint8_t)(val >> 8);
    dst[2] = (uint8_t)(val >> 16);
    dst[3] = (uint8_t)(val >> 24);
    dst[4] = (uint8_t)(val >> 32);
    dst[5] = (uint8_t)(val >> 40);
    dst[6] = (uint8_t)(val >> 48);
    dst[7] = (uint8_t)(val >> 56);
}

/* Read integer values from Little-Endian byte order */
static inline uint16_t get_le16(const uint8_t *src) {
    return ((uint16_t)src[0]) |
           ((uint16_t)src[1] << 8);
}

static inline uint32_t get_le32(const uint8_t *src) {
    return ((uint32_t)src[0])       |
           ((uint32_t)src[1] << 8)  |
           ((uint32_t)src[2] << 16) |
           ((uint32_t)src[3] << 24);
}

static inline uint64_t get_le64(const uint8_t *src) {
    return ((uint64_t)src[0])       |
           ((uint64_t)src[1] << 8)  |
           ((uint64_t)src[2] << 16) |
           ((uint64_t)src[3] << 24) |
           ((uint64_t)src[4] << 32) |
           ((uint64_t)src[5] << 40) |
           ((uint64_t)src[6] << 48) |
           ((uint64_t)src[7] << 56);
}

/* Float conversions (IEEE 754 32-bit float bit-cast) */
static inline void put_le_float(uint8_t *dst, float val) {
    uint32_t raw;
    memcpy(&raw, &val, sizeof(float));
    put_le32(dst, raw);
}

static inline float get_le_float(const uint8_t *src) {
    uint32_t raw = get_le32(src);
    float val;
    memcpy(&val, &raw, sizeof(float));
    return val;
}
```

#### 2. Serializing Chunk Header (Writer):

```c
void serialize_chunk_header_2d(uint8_t dst[44], const ChunkHeader2D *h) {
    put_le16(dst + 0,  h->magic);
    put_le16(dst + 2,  h->count);
    put_le64(dst + 4,  (uint64_t)h->start_time);
    put_le64(dst + 12, (uint64_t)h->end_time);
    put_le_float(dst + 20, h->min_x);
    put_le_float(dst + 24, h->max_x);
    put_le_float(dst + 28, h->min_y);
    put_le_float(dst + 32, h->max_y);
    put_le32(dst + 36, h->payload_size);
    put_le32(dst + 40, h->checksum);
}
```

#### 3. Deserializing Chunk Header (Reader):

```c
void deserialize_chunk_header_2d(ChunkHeader2D *h, const uint8_t src[44]) {
    h->magic        = get_le16(src + 0);
    h->count        = get_le16(src + 2);
    h->start_time   = (int64_t)get_le64(src + 4);
    h->end_time     = (int64_t)get_le64(src + 12);
    h->min_x        = get_le_float(src + 20);
    h->max_x        = get_le_float(src + 24);
    h->min_y        = get_le_float(src + 28);
    h->max_y        = get_le_float(src + 32);
    h->payload_size = get_le32(src + 36);
    h->checksum     = get_le32(src + 40);
}
```

**Key Advantages:**

1. **Zero Unaligned Accesses**: All memory reads/writes occur at individual byte granularities (`dst[i]`).
2. **Deterministic Endianness**: Files written on any CPU architecture can be read on any other CPU architecture with bit-for-bit identical results.
3. **No Compiler Pragmas**: Eliminates compiler-dependent `#pragma pack` directives.

---

### Solution B: Natural Alignment with Explicit Padding

If an engine requires zero-copy, direct-struct memory mapping without serialization loops, the disk schema itself must be engineered so that **every $N$-byte field naturally falls on a memory offset divisible by $N$**.

#### Naturally Aligned `ChunkHeader2D` (48 Bytes):

```c
typedef struct {
    uint16_t magic;         // Offset 0  (2 bytes)
    uint16_t count;         // Offset 2  (2 bytes)
    uint32_t reserved_pad;  // Offset 4  (4 bytes) -> PADS so start_time lands at offset 8!
    int64_t  start_time;    // Offset 8  (8 bytes) -> Aligned to 8
    int64_t  end_time;      // Offset 16 (8 bytes) -> Aligned to 8
    float    min_x, max_x;  // Offset 24, 28 (4 bytes each) -> Aligned to 4
    float    min_y, max_y;  // Offset 32, 36 (4 bytes each) -> Aligned to 4
    uint32_t payload_size;  // Offset 40 (4 bytes) -> Aligned to 4
    uint32_t checksum;      // Offset 44 (4 bytes) -> Aligned to 4
} ChunkHeader2D;            // Total: 48 bytes (Multiple of 8)
```

#### Naturally Aligned `Point3D` (24 Bytes):

```c
typedef struct {
    int64_t timestamp;  // Offset 0 (8 bytes) -> Aligned to 8
    float x, y, z;      // Offset 8, 12, 16 (4 bytes each)
    uint32_t reserved;  // Offset 20 (4 bytes) -> Rounds record size to 24 bytes (multiple of 8)
} Point3D;              // Total size: 24 bytes
```

In an array of 24-byte `Point3D`:

- Index 0: Offset 0 (0 % 8 == 0)
- Index 1: Offset 24 (24 % 8 == 0)
- Index 2: Offset 48 (48 % 8 == 0)

All array elements remain naturally 8-byte aligned at all times.

---

## 4. Comparison Matrix

| Property               | Direct `#pragma pack(1)` Struct Write | Naturally Aligned Struct (+Explicit Pad) | Explicit Byte Serialization (`put_le*`) |
| :--------------------- | :------------------------------------ | :--------------------------------------- | :-------------------------------------- |
| **CPU Alignment Safe** | No (Faults on ARM/SPARC/RISC-V)       | Yes (All fields aligned)                 | Yes (Byte-by-byte access)               |
| **Endianness Safe**    | No (Host-dependent)                   | No (Host-dependent)                      | Yes (Guaranteed Little-Endian)          |
| **Zero-Copy Overhead** | Yes                                   | Yes                                      | Minor loop overhead                     |
| **Portability**        | Compiler-dependent pragmas            | Limited across different endians         | 100% C99 Compliant everywhere           |
| **On-Disk Overhead**   | Minimal (no padding bytes)            | Minor (4 bytes padding per chunk)        | Minimal (no padding bytes)              |

---

## 5. Architectural Recommendation for PTDB

1. **For Metadata & Chunk Headers**:
   Use **Explicit Byte Serialization** (`serialize_chunk_header_*` / `deserialize_chunk_header_*`). Headers are small (44 to 52 bytes) and read only once per chunk (e.g., every 500 points). The CPU overhead is sub-microsecond, while offering complete immunity to endianness bugs, compiler warnings, and unaligned memory traps.

2. **For High-Throughput Bulk Payload Arrays (`Point2D` / `Point3D`)**:
   - `Point2D` (16 bytes: `int64_t` + `float` + `float`) is **already naturally aligned** because 16 is a multiple of 8.
   - For `Point3D` (20 bytes unaligned stride): either pad to 24 bytes with a reserved 4-byte slot to maintain natural alignment, or unpack points using endian-safe deserialization routines when precision and portability across different CPU architectures are mandatory.
