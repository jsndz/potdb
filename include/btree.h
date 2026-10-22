#ifndef BTREE_H
#define BTREE_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "pager.h"
#include "row.h"
#include "node.h"

typedef struct
{
    uint32_t root_page_num;
    Pager *pager;
} Table;

typedef struct
{
    Table *table;
    uint32_t page_num;
    uint32_t cell_num;
    bool end_of_table;
} Cursor;

Table *db_open(const char *fileName);
void db_close(Table *table);
void free_table(Table *table);

Cursor *table_start(Table *table);
Cursor *table_find(Table *table, time_t key);
Cursor *leaf_node_find(Table *table, uint32_t page_num, time_t key);
Cursor *internal_node_find(Table *table, time_t key, uint32_t page_num);
uint32_t internal_node_find_child(void *node, time_t key);

void *cursor_value(Cursor *cursor);
void cursor_advance(Cursor *cursor);

void leaf_node_insert(Cursor *cursor, time_t key, Row *value);

void print_tree(Pager *pager, uint32_t page_num, uint32_t indentation_level);

#endif // BTREE_H
