#include "crc32.h"

static uint32_t crc32_table[256];
static int crc32_initialized = 0;

static void init_crc32_table(void)
{
    for (uint32_t i = 0; i < 256; i++)
    {
        uint32_t c = i;
        for (int j = 0; j < 8; j++)
        {
            if (c & 1)
            {
                c = 0xEDB88320L ^ (c >> 1);
            }
            else
            {
                c = c >> 1;
            }
        }
        crc32_table[i] = c;
    }
    crc32_initialized = 1;
}

uint32_t ptdb_crc32(const void *data, size_t length)
{
    if (!crc32_initialized)
    {
        init_crc32_table();
    }
    const uint8_t *buf = (const uint8_t *)data;
    uint32_t crc = 0xFFFFFFFF;
    for (size_t i = 0; i < length; i++)
    {
        crc = crc32_table[(crc ^ buf[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFF;
}
