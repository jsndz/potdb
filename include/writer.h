#ifndef WRITER_H
#define WRITER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "chunk.h"

typedef struct PTDB PTDB;

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

#endif
