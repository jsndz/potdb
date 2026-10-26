#ifndef PTDB_H
#define PTDB_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "chunk.h"
#include "file.h"

typedef struct PTDB PTDB;
typedef struct PTDBReader PTDBReader;

int ptdb_init(const char *filepath, uint8_t dim, uint8_t time_unit);
PTDB *ptdb_open(const char *filepath, const PTDBConfig *config);
int ptdb_close(PTDB *db);
uint8_t ptdb_get_dim(const PTDB *db);

int ptdb_insert_2d(PTDB *db, int64_t timestamp, float x, float y);
int ptdb_insert_3d(PTDB *db, int64_t timestamp, float x, float y, float z);
int ptdb_write_2d(PTDB *db, int64_t timestamp, float x, float y);
int ptdb_write_3d(PTDB *db, int64_t timestamp, float x, float y, float z);
int ptdb_insert_point(PTDB *db, const void *point);
int ptdb_flush(PTDB *db);

PTDBReader *ptdb_reader_open(const char *filepath);
int ptdb_reader_next_chunk(PTDBReader *reader, void *header_out, void **points_out, uint32_t *count_out);
void ptdb_get_point_2d(const void *points, uint32_t index, Point2D *out);
void ptdb_get_point_3d(const void *points, uint32_t index, Point3D *out);
void ptdb_reader_close(PTDBReader *reader);
uint8_t ptdb_reader_get_dim(const PTDBReader *reader);

uint32_t ptdb_crc32(const void *data, size_t length);

#endif
