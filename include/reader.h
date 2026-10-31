#ifndef READER_H
#define READER_H

#include <stdint.h>
#include <stddef.h>
#include "chunk.h"

typedef struct PTDBReader PTDBReader;

typedef struct {
    int64_t min_time;
    int64_t max_time;
    float min_x, max_x;
    float min_y, max_y;
} PTDBQueryRange2D;

typedef struct {
    int64_t min_time;
    int64_t max_time;
    float min_x, max_x;
    float min_y, max_y;
    float min_z, max_z;
} PTDBQueryRange3D;

typedef void (*ptdb_point_2d_callback)(const Point2D *point, void *user_data);
typedef void (*ptdb_point_3d_callback)(const Point3D *point, void *user_data);

PTDBReader *ptdb_reader_open(const char *filepath);
int ptdb_reader_next_chunk(PTDBReader *reader, void *header_out, void **points_out, uint32_t *count_out);
void ptdb_get_point_2d(const void *points, uint32_t index, Point2D *out);
void ptdb_get_point_3d(const void *points, uint32_t index, Point3D *out);
void ptdb_reader_close(PTDBReader *reader);
uint8_t ptdb_reader_get_dim(const PTDBReader *reader);

uint64_t ptdb_query_range_2d(const char *filepath, const PTDBQueryRange2D *range, ptdb_point_2d_callback callback, void *user_data);
uint64_t ptdb_query_range_3d(const char *filepath, const PTDBQueryRange3D *range, ptdb_point_3d_callback callback, void *user_data);

#endif
