#ifndef CRC32_H
#define CRC32_H

#include <stdint.h>
#include <stddef.h>

uint32_t ptdb_crc32(const void *data, size_t length);

#endif
