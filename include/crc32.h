#ifndef CRC32_H
#define CRC32_H

#include <stdint.h>
#include <stddef.h>

uint32_t ptdb_crc32(const void *data, size_t length);
uint32_t ptdb_crc32_update(uint32_t crc, const void *data, size_t length);

#endif
