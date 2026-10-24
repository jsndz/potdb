#ifndef TABLE_H
#define TABLE_H

#include <stdint.h>
#include <sys/types.h>
#include "row.h"

typedef struct
{
    int file_descriptor;
    off_t file_length;
    uint32_t num_rows;
} Table;

Table *db_open(const char *filename);
void db_close(Table *table);
void table_append_row(Table *table, Row *row);
void table_select_all(Table *table);

#endif // TABLE_H
