#ifndef SERIALIZE_H
#define SERIALIZE_H

#include <stdint.h>
#include <string.h>
#include "chunk.h"

static inline void put_le16(uint8_t *dst, uint16_t val)
{
    dst[0] = (uint8_t)(val & 0xFF);
    dst[1] = (uint8_t)((val >> 8) & 0xFF);
}

static inline void put_le32(uint8_t *dst, uint32_t val)
{
    dst[0] = (uint8_t)(val & 0xFF);
    dst[1] = (uint8_t)((val >> 8) & 0xFF);
    dst[2] = (uint8_t)((val >> 16) & 0xFF);
    dst[3] = (uint8_t)((val >> 24) & 0xFF);
}

static inline void put_le64(uint8_t *dst, uint64_t val)
{
    dst[0] = (uint8_t)(val & 0xFF);
    dst[1] = (uint8_t)((val >> 8) & 0xFF);
    dst[2] = (uint8_t)((val >> 16) & 0xFF);
    dst[3] = (uint8_t)((val >> 24) & 0xFF);
    dst[4] = (uint8_t)((val >> 32) & 0xFF);
    dst[5] = (uint8_t)((val >> 40) & 0xFF);
    dst[6] = (uint8_t)((val >> 48) & 0xFF);
    dst[7] = (uint8_t)((val >> 56) & 0xFF);
}

static inline uint16_t get_le16(const uint8_t *src)
{
    return (uint16_t)(((uint16_t)src[0]) |
                      ((uint16_t)src[1] << 8));
}

static inline uint32_t get_le32(const uint8_t *src)
{
    return ((uint32_t)src[0])       |
           ((uint32_t)src[1] << 8)  |
           ((uint32_t)src[2] << 16) |
           ((uint32_t)src[3] << 24);
}

static inline uint64_t get_le64(const uint8_t *src)
{
    return ((uint64_t)src[0])       |
           ((uint64_t)src[1] << 8)  |
           ((uint64_t)src[2] << 16) |
           ((uint64_t)src[3] << 24) |
           ((uint64_t)src[4] << 32) |
           ((uint64_t)src[5] << 40) |
           ((uint64_t)src[6] << 48) |
           ((uint64_t)src[7] << 56);
}

static inline void put_le_float(uint8_t *dst, float val)
{
    uint32_t raw;
    memcpy(&raw, &val, sizeof(float));
    put_le32(dst, raw);
}

static inline float get_le_float(const uint8_t *src)
{
    uint32_t raw = get_le32(src);
    float val;
    memcpy(&val, &raw, sizeof(float));
    return val;
}

static inline void serialize_file_header(uint8_t *dst, const FileHeader *h)
{
    put_le32(dst + 0, h->magic);
    put_le16(dst + 4, h->version);
    dst[6] = h->dim;
    dst[7] = h->time_unit;
    put_le64(dst + 8, h->reserved);
}

static inline void deserialize_file_header(FileHeader *h, const uint8_t *src)
{
    h->magic     = get_le32(src + 0);
    h->version   = get_le16(src + 4);
    h->dim       = src[6];
    h->time_unit = src[7];
    h->reserved  = get_le64(src + 8);
}

static inline void serialize_chunk_header_2d(uint8_t *dst, const ChunkHeader2D *h)
{
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

static inline void deserialize_chunk_header_2d(ChunkHeader2D *h, const uint8_t *src)
{
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

static inline void serialize_chunk_header_3d(uint8_t *dst, const ChunkHeader3D *h)
{
    put_le16(dst + 0,  h->magic);
    put_le16(dst + 2,  h->count);
    put_le64(dst + 4,  (uint64_t)h->start_time);
    put_le64(dst + 12, (uint64_t)h->end_time);
    put_le_float(dst + 20, h->min_x);
    put_le_float(dst + 24, h->max_x);
    put_le_float(dst + 28, h->min_y);
    put_le_float(dst + 32, h->max_y);
    put_le_float(dst + 36, h->min_z);
    put_le_float(dst + 40, h->max_z);
    put_le32(dst + 44, h->payload_size);
    put_le32(dst + 48, h->checksum);
}

static inline void deserialize_chunk_header_3d(ChunkHeader3D *h, const uint8_t *src)
{
    h->magic        = get_le16(src + 0);
    h->count        = get_le16(src + 2);
    h->start_time   = (int64_t)get_le64(src + 4);
    h->end_time     = (int64_t)get_le64(src + 12);
    h->min_x        = get_le_float(src + 20);
    h->max_x        = get_le_float(src + 24);
    h->min_y        = get_le_float(src + 28);
    h->max_y        = get_le_float(src + 32);
    h->min_z        = get_le_float(src + 36);
    h->max_z        = get_le_float(src + 40);
    h->payload_size = get_le32(src + 44);
    h->checksum     = get_le32(src + 48);
}

#endif
