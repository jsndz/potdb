#ifndef BTREE_H
#define BTREE_H

#include <stdint.h>
#include <stdbool.h>
#include "row.h"
#include "pager.h"
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
Cursor *table_find(Table *table, uint32_t key);
Cursor *leaf_node_find(Table *table, uint32_t page_num, uint32_t key);
Cursor *internal_node_find(Table *table, uint32_t key, uint32_t page_num);
uint32_t internal_node_find_child(void *node, uint32_t key);
void update_internal_node_key(void *node, uint32_t old_key, uint32_t new_key);

void *cursor_value(Cursor *cursor);
void cursor_advance(Cursor *cursor);

void leaf_node_insert(Cursor *cursor, uint32_t key, Row *value);
void leaf_node_split_and_insert(Cursor *cursor, uint32_t key, Row *value);
void internal_node_insert(Table *table, uint32_t parent_page_num, uint32_t child_page_num);
void internal_node_split_and_insert(Table *table, uint32_t parent_page_num, uint32_t child_page_num);
void create_new_root_node(Table *table, uint32_t right_child_page_num);

void print_tree(Pager *pager, uint32_t page_num, uint32_t indentation_level);

#endif // BTREE_H
