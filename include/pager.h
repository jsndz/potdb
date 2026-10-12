#ifndef PAGER_H
#define PAGER_H

#include <stdint.h>
#include <stddef.h>
 
#define PAGE_SIZE 4096
#define TABLE_MAX_PAGES 100

typedef struct
{
    int file_descriptor;
    uint32_t file_length;
    uint32_t num_pages;
    void *pages[TABLE_MAX_PAGES];
} Pager;

Pager *pager_open(const char *filename);
void *get_page(Pager *pager, uint32_t page_num);
uint32_t get_unused_pages(Pager *pager);
void pager_flush(Pager *pager, uint32_t page_num);
void pager_flush_all(Pager *pager);
void pager_reload_page(Pager *pager, uint32_t page_num);
void pager_truncate(Pager *pager, uint32_t new_num_pages);
void pager_close(Pager *pager);

#endif // PAGER_H
