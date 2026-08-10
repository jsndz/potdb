#include "btree.h"
#include "repl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
    Pager *pager = table->pager;
    for (uint32_t i = 0; i < pager->num_pages; i++)
    {
        if (pager->pages[i] == NULL)
        {
            continue;
        }
        pager_flush(pager, i);
        free(pager->pages[i]);
        pager->pages[i] = NULL;
    }

    int result = close(pager->file_descriptor);
    if (result == -1)
    {
        printf("error in closing.\n");
        exit(EXIT_FAILURE);
    }
    for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++)
    {
        void *page = pager->pages[i];
        if (page)
        {
            free(page);
            pager->pages[i] = NULL;
        }
    }
    free(pager);
    free(table);
}

void free_table(Table *table)
{
    if (table == NULL) return;
    if (table->pager != NULL)
    {
        for (uint32_t i = 0; i < TABLE_MAX_PAGES; i++)
        {
            if (table->pager->pages[i] != NULL)
            {
                free(table->pager->pages[i]);
                table->pager->pages[i] = NULL;
            }
        }
        free(table->pager);
    }
    free(table);
}


Cursor *leaf_node_find(Table *table, uint32_t page_num, uint32_t key)
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
        uint32_t key_at_index = *leaf_node_key(node, index);
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

uint32_t internal_node_find_child(void *node, uint32_t key)
{
    uint32_t num_keys = *internal_node_num_key(node);
    uint32_t min_index = 0;
    uint32_t max_index = num_keys;

    while (max_index != min_index)
    {
        uint32_t index = (min_index + max_index) / 2;
        uint32_t key_to_right = *internal_node_key(node, index);

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

void update_internal_node_key(void *node, uint32_t old_key, uint32_t new_key)
{
    uint32_t old_child_index = internal_node_find_child(node, old_key);
    *internal_node_key(node, old_child_index) = new_key;
}

Cursor *internal_node_find(Table *table, uint32_t key, uint32_t page_num)
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

Cursor *table_find(Table *table, uint32_t key)
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

void create_new_root_node(Table *table, uint32_t right_child_page_num)
{
    void *root = get_page(table->pager, table->root_page_num);
    void *right_child = get_page(table->pager, right_child_page_num);
    uint32_t left_child_page_num = get_unused_pages(table->pager);
    void *left_child = get_page(table->pager, left_child_page_num);

    if (get_node_type(root) == NODE_INTERNAL)
    {
        initialize_internal_node(right_child);
        initialize_internal_node(left_child);
    }
    memcpy(left_child, root, PAGE_SIZE);
    set_root_node(left_child, false);

    if (get_node_type(left_child) == NODE_INTERNAL)
    {
        void *child;
        for (uint32_t i = 0; i < *internal_node_num_key(left_child); i++)
        {
            child = get_page(table->pager, *internal_node_child(left_child, i));
            *node_parent(child) = left_child_page_num;
        }
        child = get_page(table->pager, *internal_node_right_child(left_child));
        *node_parent(child) = left_child_page_num;
    }

    initialize_internal_node(root);
    set_root_node(root, true);
    *internal_node_num_key(root) = 1;

    *internal_node_child(root, 0) = left_child_page_num;
    uint32_t left_child_max_key = get_node_max_key(table->pager, left_child);

    *internal_node_key(root, 0) = left_child_max_key;
    *internal_node_right_child(root) = right_child_page_num;
    *node_parent(left_child) = table->root_page_num;
    *node_parent(right_child) = table->root_page_num;
}

void internal_node_insert(Table *table, uint32_t parent_page_num, uint32_t child_page_num)
{
    void *parent = get_page(table->pager, parent_page_num);
    void *child = get_page(table->pager, child_page_num);
    uint32_t child_max_key = get_node_max_key(table->pager, child);
    uint32_t index = internal_node_find_child(parent, child_max_key);
    uint32_t original_num_keys = *internal_node_num_key(parent);
    if (original_num_keys >= INTERNAL_NODE_MAX_KEYS)
    {
        internal_node_split_and_insert(table, parent_page_num, child_page_num);
        return;
    }

    uint32_t right_child_page_num = *internal_node_right_child(parent);
    if (right_child_page_num == INVALID_PAGE_NUM)
    {
        *internal_node_right_child(parent) = child_page_num;
        return;
    }
    *internal_node_num_key(parent) = original_num_keys + 1;
    void *right_child = get_page(table->pager, right_child_page_num);
    if (child_max_key > get_node_max_key(table->pager, right_child))
    {
        *internal_node_child(parent, original_num_keys) = right_child_page_num;
        *internal_node_key(parent, original_num_keys) = get_node_max_key(table->pager, right_child);
        *internal_node_right_child(parent) = child_page_num;
    }
    else
    {
        for (uint32_t i = original_num_keys; i > index; i--)
        {
            void *source = internal_node_cell(parent, i - 1);
            void *destination = internal_node_cell(parent, i);
            memcpy(destination, source, INTERNAL_NODE_CELL_SIZE);
        }
        *internal_node_child(parent, index) = child_page_num;
        *internal_node_key(parent, index) = child_max_key;
    }
}

void leaf_node_split_and_insert(Cursor *cursor, uint32_t key, Row *value)
{
    void *old_node = get_page(cursor->table->pager, cursor->page_num);
    uint32_t new_page_num = get_unused_pages(cursor->table->pager);
    void *new_node = get_page(cursor->table->pager, new_page_num);
    initialize_leaf_node(new_node);
    *node_parent(new_node) = *node_parent(old_node);
    *leaf_node_next_leaf(new_node) = *leaf_node_next_leaf(old_node);
    *leaf_node_next_leaf(old_node) = new_page_num;

    for (int32_t i = LEAF_NODE_MAX_CELLS; i >= 0; i--)
    {
        void *destination_node;
        if (i >= (int32_t)LEAF_NODE_LEFT_SPLIT_COUNT)
        {
            destination_node = new_node;
        }
        else
        {
            destination_node = old_node;
        }
        uint32_t index_within_node = i % LEAF_NODE_LEFT_SPLIT_COUNT;
        void *destination = leaf_node_cell(destination_node, index_within_node);
        if ((uint32_t)i == cursor->cell_num)
        {
            serialize_row(value, leaf_node_value(destination_node, index_within_node));
            *leaf_node_key(destination_node, index_within_node) = key;
        }
        else if ((uint32_t)i > cursor->cell_num)
        {
            memcpy(destination, leaf_node_cell(old_node, i - 1), LEAF_NODE_CELL_SIZE);
        }
        else
        {
            memcpy(destination, leaf_node_cell(old_node, i), LEAF_NODE_CELL_SIZE);
        }
    }

    *(leaf_node_num_cells(old_node)) = LEAF_NODE_LEFT_SPLIT_COUNT;
    *(leaf_node_num_cells(new_node)) = LEAF_NODE_RIGHT_SPLIT_COUNT;

    if (is_root_node(old_node))
    {
        create_new_root_node(cursor->table, new_page_num);
    }
    else
    {
        uint32_t parent_page_num = *node_parent(old_node);
        uint32_t old_max = get_node_max_key(cursor->table->pager, old_node);
        uint32_t new_max = get_node_max_key(cursor->table->pager, old_node);
        void *parent = get_page(cursor->table->pager, parent_page_num);
        update_internal_node_key(parent, old_max, new_max);
        internal_node_insert(cursor->table, parent_page_num, new_page_num);
    }
}

void leaf_node_insert(Cursor *cursor, uint32_t key, Row *value)
{
    void *node = get_page(cursor->table->pager, cursor->page_num);
    uint32_t num_cells = *leaf_node_num_cells(node);

    if (num_cells >= LEAF_NODE_MAX_CELLS)
    {
        printf("Leaf node full, splitting...\n");
        leaf_node_split_and_insert(cursor, key, value);
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

    printf("Inserted key %d at cell %d\n", key, cursor->cell_num);
}

void internal_node_split_and_insert(Table *table, uint32_t parent_page_num, uint32_t child_page_num)
{
    uint32_t old_page_num = parent_page_num;
    void *old_node = get_page(table->pager, parent_page_num);
    uint32_t old_max = get_node_max_key(table->pager, old_node);

    void *child = get_page(table->pager, child_page_num);
    uint32_t child_max = get_node_max_key(table->pager, child);

    uint32_t new_page_num = get_unused_pages(table->pager);
    uint32_t splitting_root_node = is_root_node(old_node);

    void *parent;
    void *new_node;
    if (splitting_root_node)
    {
        create_new_root_node(table, new_page_num);
        parent = get_page(table->pager, table->root_page_num);
        old_page_num = *internal_node_child(parent, 0);
        old_node = get_page(table->pager, old_page_num);
    }
    else
    {
        parent = get_page(table->pager, *node_parent(old_node));
        new_node = get_page(table->pager, new_page_num);
        initialize_internal_node(new_node);
    }

    uint32_t *old_num_keys = internal_node_num_key(old_node);
    uint32_t cur_page_num = *internal_node_right_child(old_node);
    void *cur = get_page(table->pager, cur_page_num);
    internal_node_insert(table, new_page_num, cur_page_num);
    *node_parent(cur) = new_page_num;
    *internal_node_right_child(old_node) = INVALID_PAGE_NUM;
    for (int i = INTERNAL_NODE_MAX_KEYS - 1; i > (int)(INTERNAL_NODE_MAX_KEYS / 2); i--)
    {
        cur_page_num = *internal_node_child(old_node, i);
        cur = get_page(table->pager, cur_page_num);
        internal_node_insert(table, new_page_num, cur_page_num);
        *node_parent(cur) = new_page_num;
        (*old_num_keys)--;
    }

    *internal_node_right_child(old_node) = *internal_node_child(old_node, *old_num_keys - 1);
    (*old_num_keys)--;
    uint32_t max_after_split = get_node_max_key(table->pager, old_node);
    uint32_t destination_page_num = child_max < max_after_split ? old_page_num : new_page_num;
    internal_node_insert(table, destination_page_num, child_page_num);
    *node_parent(child) = destination_page_num;
    update_internal_node_key(parent, old_max, get_node_max_key(table->pager, old_node));
    if (!splitting_root_node)
    {
        internal_node_insert(table, *node_parent(old_node), new_page_num);
        *node_parent(new_node) = *node_parent(old_node);
    }
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
            printf("- %d\n", *leaf_node_key(node, i));
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
