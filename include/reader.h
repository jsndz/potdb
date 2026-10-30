#ifndef READER_H
#define READER_H

#include <stdint.h>
#include <stddef.h>
#include "chunk.h"

typedef struct PTDBReader PTDBReader;

PTDBReader *ptdb_reader_open(const char *filepath);
int ptdb_reader_next_chunk(PTDBReader *reader, void *header_out, void **points_out, uint32_t *count_out);
void ptdb_get_point_2d(const void *points, uint32_t index, Point2D *out);
void ptdb_get_point_3d(const void *points, uint32_t index, Point3D *out);
void ptdb_reader_close(PTDBReader *reader);
uint8_t ptdb_reader_get_dim(const PTDBReader *reader);

#endif
