#include "btree.h"
#include "pager.h"
#include "repl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

Table *db_open(const char *fileName)
{
    Pager *pager = pager_open(fileName);
    Table *table = malloc(sizeof(Table));
    table->pager = pager;
    table->root_page_num = 0;
    if (pager->num_pages == 0)
    {
        void *root_node = get_page(pager, 0);
        initialize_leaf_node(root_node);
        set_root_node(root_node, true);
    }
    return table;
}

void db_close(Table *table)
{
    if (table == NULL) return;
    if (table->pager != NULL)
    {
        pager_close(table->pager);
        table->pager = NULL;
    }
    free(table);
}

void free_table(Table *table)
{
    db_close(table);
}

Cursor *leaf_node_find(Table *table, uint32_t page_num, time_t key)
{
    void *node = get_page(table->pager, page_num);
    uint32_t num_cells = *leaf_node_num_cells(node);
    Cursor *cursor = malloc(sizeof(Cursor));
    cursor->table = table;
    cursor->page_num = page_num;

    uint32_t min_index = 0;
    uint32_t one_past_max_index = num_cells;
    while (one_past_max_index != min_index)
    {
        uint32_t index = (min_index + one_past_max_index) / 2;
        time_t key_at_index = *leaf_node_key(node, index);
        if (key == key_at_index)
        {
            cursor->cell_num = index;
            return cursor;
        }
        if (key < key_at_index)
        {
            one_past_max_index = index;
        }
        else
        {
            min_index = index + 1;
        }
    }
    cursor->cell_num = min_index;
    return cursor;
}

uint32_t internal_node_find_child(void *node, time_t key)
{
    uint32_t num_keys = *internal_node_num_key(node);
    uint32_t min_index = 0;
    uint32_t max_index = num_keys;

    while (max_index != min_index)
    {
        uint32_t index = (min_index + max_index) / 2;
        time_t key_to_right = *internal_node_key(node, index);

        if (key <= key_to_right)
        {
            max_index = index;
        }
        else
        {
            min_index = index + 1;
        }
    }
    return min_index;
}

Cursor *internal_node_find(Table *table, time_t key, uint32_t page_num)
{
    void *node = get_page(table->pager, page_num);
    uint32_t child_index = internal_node_find_child(node, key);
    uint32_t child_num = *internal_node_child(node, child_index);
    void *child = get_page(table->pager, child_num);

    switch (get_node_type(child))
    {
    case NODE_LEAF:
        return leaf_node_find(table, child_num, key);
    case NODE_INTERNAL:
        return internal_node_find(table, key, child_num);
    default:
        exit(EXIT_FAILURE);
    }
}

Cursor *table_find(Table *table, time_t key)
{
    uint32_t root_page_num = table->root_page_num;
    void *root_node = get_page(table->pager, root_page_num);
    if (get_node_type(root_node) == NODE_LEAF)
    {
        return leaf_node_find(table, root_page_num, key);
    }
    else
    {
        return internal_node_find(table, key, root_page_num);
    }
}

Cursor *table_start(Table *table)
{
    Cursor *cursor = table_find(table, 0);
    void *node = get_page(table->pager, cursor->page_num);
    uint32_t num_cells = *leaf_node_num_cells(node);
    cursor->end_of_table = (num_cells == 0);
    return cursor;
}

void *cursor_value(Cursor *cursor)
{
    uint32_t page_num = cursor->page_num;
    void *page = get_page(cursor->table->pager, page_num);
    return leaf_node_value(page, cursor->cell_num);
}

void cursor_advance(Cursor *cursor)
{
    uint32_t page_num = cursor->page_num;
    void *node = get_page(cursor->table->pager, page_num);
    cursor->cell_num += 1;

    if (cursor->cell_num >= (*leaf_node_num_cells(node)))
    {
        uint32_t next_page_num = *leaf_node_next_leaf(node);
        if (next_page_num == 0)
        {
            cursor->end_of_table = true;
        }
        else
        {
            cursor->page_num = next_page_num;
            cursor->cell_num = 0;
        }
    }
}

void leaf_node_insert(Cursor *cursor, time_t key, Row *value)
{
    void *node = get_page(cursor->table->pager, cursor->page_num);
    uint32_t num_cells = *leaf_node_num_cells(node);

    if (num_cells >= LEAF_NODE_MAX_CELLS)
    {
        printf("Leaf node full.\n");
        return;
    }

    if (cursor->cell_num < num_cells)
    {
        for (uint32_t i = num_cells; i > cursor->cell_num; i--)
        {
            memcpy(leaf_node_cell(node, i), leaf_node_cell(node, i - 1), LEAF_NODE_CELL_SIZE);
        }
    }
    *(leaf_node_num_cells(node)) += 1;
    *(leaf_node_key(node, cursor->cell_num)) = key;
    serialize_row(value, leaf_node_value(node, cursor->cell_num));

    printf("Inserted key %ld at cell %d\n", (long)key, cursor->cell_num);
}

void print_tree(Pager *pager, uint32_t page_num, uint32_t indentation_level)
{
    void *node = get_page(pager, page_num);
    uint32_t num_keys, child;

    switch (get_node_type(node))
    {
    case NODE_LEAF:
        num_keys = *leaf_node_num_cells(node);
        indent(indentation_level);
        printf("- leaf (size %d)\n", num_keys);

        for (uint32_t i = 0; i < num_keys; i++)
        {
            indent(indentation_level + 1);
            printf("- %ld\n", (long)*leaf_node_key(node, i));
        }
        break;

    case NODE_INTERNAL:
        num_keys = *internal_node_num_key(node);
        indent(indentation_level);
        printf("- internal (size %d)\n", num_keys);

        if (num_keys > 0)
        {
            for (uint32_t i = 0; i < num_keys; i++)
            {
                child = *internal_node_child(node, i);
                print_tree(pager, child, indentation_level + 1);
            }
        }
        break;
    }
}
