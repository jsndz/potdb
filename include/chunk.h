#ifndef CHUNK_H
#define CHUNK_H

#include <stdint.h>

#define PTDB_FILE_MAGIC 0x42445450
#define PTDB_CHUNK_MAGIC 0x4348
#define PTDB_VERSION 1

#define PTDB_DEFAULT_MAX_POINTS 500
#define PTDB_DEFAULT_MAX_TIME_WINDOW_MS 5000

#define PTDB_TIME_UNIT_MS 1
#define PTDB_TIME_UNIT_SEC 2
#define PTDB_TIME_UNIT_US 3
#define PTDB_TIME_UNIT_NS 4

#pragma pack(push, 1)

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint8_t  dim;
    uint8_t  time_unit;
    uint64_t reserved;
} FileHeader;

typedef struct {
    uint16_t magic;
    uint16_t count;
    int64_t  start_time;
    int64_t  end_time;
    float    min_x, max_x;
    float    min_y, max_y;
    uint32_t payload_size;
    uint32_t checksum;
} ChunkHeader2D;

typedef struct {
    uint16_t magic;
    uint16_t count;
    int64_t  start_time;
    int64_t  end_time;
    float    min_x, max_x;
    float    min_y, max_y;
    float    min_z, max_z;
    uint32_t payload_size;
    uint32_t checksum;
} ChunkHeader3D;

#pragma pack(pop)

#define FILE_HEADER_SIZE sizeof(FileHeader)
#define CHUNK_HEADER_2D_SIZE sizeof(ChunkHeader2D)
#define CHUNK_HEADER_3D_SIZE sizeof(ChunkHeader3D)

typedef struct {
    int64_t timestamp;
    float x, y;
} Point2D;

typedef struct {
    int64_t timestamp;
    float x, y, z;
} Point3D;

typedef struct {
    uint32_t max_points;
    uint32_t max_time_window_ms;
} PTDBConfig;

#endif
