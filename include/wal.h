#ifndef WAL_H
#define WAL_H

#include <stdint.h>
#include <stddef.h>
#include "statement.h"
#include "row.h"

#define WAL_FRAME_SIZE 4096
#define WAL_MAX_PAGES 100

typedef struct {
    int file_descriptor;
    uint32_t num_pages;
    uint32_t file_length;
    void *log_pages[WAL_MAX_PAGES];
} Logger;

typedef struct {
    uint32_t LSN;
    uint32_t txn;
    StatementType type;
    uint32_t PageId;
    Row *row;
} Log;

// Log Serialization Sizes & Offsets
#define LSN_SIZE ((uint32_t)sizeof(uint32_t))
#define TXN_SIZE ((uint32_t)sizeof(uint32_t))
#define PAGE_ID_SIZE ((uint32_t)sizeof(uint32_t))
#define TYPE_SIZE ((uint32_t)sizeof(uint32_t))
#define LOG_ROW_SIZE (ROW_SIZE)

#define LSN_OFFSET ((uint32_t)0)
#define TXN_OFFSET (LSN_OFFSET + LSN_SIZE)
#define PAGE_ID_OFFSET (TXN_OFFSET + TXN_SIZE)
#define TYPE_OFFSET (PAGE_ID_OFFSET + PAGE_ID_SIZE)
#define LOG_ROW_OFFSET (TYPE_OFFSET + TYPE_SIZE)
#define LOG_SIZE (LOG_ROW_OFFSET + LOG_ROW_SIZE)

void serialize_log(Log *source, void *destination);
void deserialize_log(void *source, Log *destination);

#endif // WAL_H