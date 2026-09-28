#ifndef WAL_H
#define WAL_H


#include <stdint.h>
#include <stddef.h>

#define WAL_FRAME_SIZE 4096
#define WAL_MAX_PAGES 100

typedef struct {
    int file_descriptor;
    uint32_t num_pages;
    uint32_t file_length;
    void *log_pages[WAL_MAX_PAGES];
} Logger;


typedef struct {
    int LSN;
}Log;

#endif