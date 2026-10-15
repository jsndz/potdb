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

void update_internal_node_key(void *node, time_t old_key, time_t new_key)
{
    uint32_t old_child_index = internal_node_find_child(node, old_key);
    *internal_node_key(node, old_child_index) = new_key;
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
    time_t left_child_max_key = get_node_max_key(table->pager, left_child);

    *internal_node_key(root, 0) = left_child_max_key;
    *internal_node_right_child(root) = right_child_page_num;
    *node_parent(left_child) = table->root_page_num;
    *node_parent(right_child) = table->root_page_num;
}

void internal_node_insert(Table *table, uint32_t parent_page_num, uint32_t child_page_num)
{
    void *parent = get_page(table->pager, parent_page_num);
    void *child = get_page(table->pager, child_page_num);
    time_t child_max_key = get_node_max_key(table->pager, child);
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

void leaf_node_split_and_insert(Cursor *cursor, time_t key, Row *value)
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
        time_t old_max = get_node_max_key(cursor->table->pager, old_node);
        time_t new_max = get_node_max_key(cursor->table->pager, old_node);
        void *parent = get_page(cursor->table->pager, parent_page_num);
        update_internal_node_key(parent, old_max, new_max);
        internal_node_insert(cursor->table, parent_page_num, new_page_num);
    }
}

void leaf_node_insert(Cursor *cursor, time_t key, Row *value)
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

    printf("Inserted key %ld at cell %d\n", (long)key, cursor->cell_num);
}

static void internal_node_remove_child(Table *table, uint32_t parent_page_num, uint32_t child_page_num);
static void internal_node_handle_underflow(Table *table, uint32_t page_num);

static uint32_t internal_node_find_child_index(void *parent, uint32_t child_page_num)
{
    uint32_t num_keys = *internal_node_num_key(parent);
    for (uint32_t i = 0; i < num_keys; i++)
    {
        if (*internal_node_cell(parent, i) == child_page_num)
        {
            return i;
        }
    }
    uint32_t *right_child = internal_node_right_child(parent);
    if (*right_child != INVALID_PAGE_NUM && *right_child == child_page_num)
    {
        return num_keys;
    }
    return INVALID_PAGE_NUM;
}

static void update_ancestor_keys(Table *table, uint32_t page_num, time_t old_max, time_t new_max)
{
    void *node = get_page(table->pager, page_num);
    if (is_root_node(node))
    {
        return;
    }

    uint32_t parent_page_num = *node_parent(node);
    void *parent = get_page(table->pager, parent_page_num);
    uint32_t num_keys = *internal_node_num_key(parent);
    uint32_t child_index = internal_node_find_child_index(parent, page_num);

    if (child_index == INVALID_PAGE_NUM)
    {
        return;
    }

    if (child_index < num_keys)
    {

        *internal_node_key(parent, child_index) = new_max;
    }
    else
    {

        update_ancestor_keys(table, parent_page_num, old_max, new_max);
    }
}

static void internal_node_handle_underflow(Table *table, uint32_t page_num)
{
    void *node = get_page(table->pager, page_num);
    if (is_root_node(node))
    {
        return;
    }

    uint32_t parent_page = *node_parent(node);
    void *parent = get_page(table->pager, parent_page);
    uint32_t p_keys = *internal_node_num_key(parent);
    uint32_t child_idx = internal_node_find_child_index(parent, page_num);

    if (child_idx > 0)
    {
        uint32_t left_sibling_page = *internal_node_child(parent, child_idx - 1);
        void *left_sibling = get_page(table->pager, left_sibling_page);
        uint32_t left_keys = *internal_node_num_key(left_sibling);

        if (left_keys > INTERNAL_NODE_MIN_KEYS)
        {

            uint32_t borrowed_page = *internal_node_right_child(left_sibling);
            void *borrowed_child = get_page(table->pager, borrowed_page);
            *node_parent(borrowed_child) = page_num;

            uint32_t node_keys = *internal_node_num_key(node);
            for (uint32_t i = node_keys; i > 0; i--)
            {
                memcpy(internal_node_cell(node, i), internal_node_cell(node, i - 1), INTERNAL_NODE_CELL_SIZE);
            }

            *internal_node_child(node, 0) = borrowed_page;
            *internal_node_key(node, 0) = get_node_max_key(table->pager, borrowed_child);
            *internal_node_num_key(node) = node_keys + 1;

            *internal_node_right_child(left_sibling) = *internal_node_child(left_sibling, left_keys - 1);
            *internal_node_num_key(left_sibling) = left_keys - 1;

            *internal_node_key(parent, child_idx - 1) = get_node_max_key(table->pager, left_sibling);
            return;
        }
    }

    if (child_idx < p_keys)
    {
        uint32_t right_sibling_page = *internal_node_child(parent, child_idx + 1);
        void *right_sibling = get_page(table->pager, right_sibling_page);
        uint32_t right_keys = *internal_node_num_key(right_sibling);

        if (right_keys > INTERNAL_NODE_MIN_KEYS)
        {
            uint32_t node_keys = *internal_node_num_key(node);
            uint32_t old_right_page = *internal_node_right_child(node);
            void *old_right_child = get_page(table->pager, old_right_page);

            *internal_node_child(node, node_keys) = old_right_page;
            *internal_node_key(node, node_keys) = get_node_max_key(table->pager, old_right_child);

            uint32_t borrowed_page = *internal_node_child(right_sibling, 0);
            void *borrowed_child = get_page(table->pager, borrowed_page);
            *node_parent(borrowed_child) = page_num;
            *internal_node_right_child(node) = borrowed_page;
            *internal_node_num_key(node) = node_keys + 1;

            for (uint32_t i = 0; i < right_keys - 1; i++)
            {
                memcpy(internal_node_cell(right_sibling, i), internal_node_cell(right_sibling, i + 1), INTERNAL_NODE_CELL_SIZE);
            }
            *internal_node_num_key(right_sibling) = right_keys - 1;

            *internal_node_key(parent, child_idx) = get_node_max_key(table->pager, node);
            return;
        }
    }

    if (child_idx > 0)
    {

        uint32_t left_sibling_page = *internal_node_child(parent, child_idx - 1);
        void *left_sibling = get_page(table->pager, left_sibling_page);
        uint32_t left_keys = *internal_node_num_key(left_sibling);

        uint32_t left_right_page = *internal_node_right_child(left_sibling);
        void *left_right_child = get_page(table->pager, left_right_page);
        *internal_node_child(left_sibling, left_keys) = left_right_page;
        *internal_node_key(left_sibling, left_keys) = get_node_max_key(table->pager, left_right_child);
        left_keys++;

        uint32_t node_keys = *internal_node_num_key(node);
        for (uint32_t i = 0; i < node_keys; i++)
        {
            uint32_t ch_page = *internal_node_child(node, i);
            void *ch = get_page(table->pager, ch_page);
            *node_parent(ch) = left_sibling_page;

            *internal_node_child(left_sibling, left_keys) = ch_page;
            *internal_node_key(left_sibling, left_keys) = *internal_node_key(node, i);
            left_keys++;
        }

        uint32_t node_right_page = *internal_node_right_child(node);
        void *node_right_child = get_page(table->pager, node_right_page);
        *node_parent(node_right_child) = left_sibling_page;
        *internal_node_right_child(left_sibling) = node_right_page;
        *internal_node_num_key(left_sibling) = left_keys;

        internal_node_remove_child(table, parent_page, page_num);
    }
    else
    {

        uint32_t right_sibling_page = *internal_node_child(parent, 1);
        void *right_sibling = get_page(table->pager, right_sibling_page);
        uint32_t node_keys = *internal_node_num_key(node);

        uint32_t node_right_page = *internal_node_right_child(node);
        void *node_right_child = get_page(table->pager, node_right_page);
        *internal_node_child(node, node_keys) = node_right_page;
        *internal_node_key(node, node_keys) = get_node_max_key(table->pager, node_right_child);
        node_keys++;

        uint32_t right_keys = *internal_node_num_key(right_sibling);
        for (uint32_t i = 0; i < right_keys; i++)
        {
            uint32_t ch_page = *internal_node_child(right_sibling, i);
            void *ch = get_page(table->pager, ch_page);
            *node_parent(ch) = page_num;

            *internal_node_child(node, node_keys) = ch_page;
            *internal_node_key(node, node_keys) = *internal_node_key(right_sibling, i);
            node_keys++;
        }

        uint32_t right_right_page = *internal_node_right_child(right_sibling);
        void *right_right_child = get_page(table->pager, right_right_page);
        *node_parent(right_right_child) = page_num;
        *internal_node_right_child(node) = right_right_page;
        *internal_node_num_key(node) = node_keys;

        internal_node_remove_child(table, parent_page, right_sibling_page);
    }
}

static void internal_node_remove_child(Table *table, uint32_t parent_page_num, uint32_t child_page_num)
{
    void *parent = get_page(table->pager, parent_page_num);
    uint32_t num_keys = *internal_node_num_key(parent);
    uint32_t child_idx = internal_node_find_child_index(parent, child_page_num);

    if (child_idx == INVALID_PAGE_NUM)
    {
        return;
    }

    if (child_idx == num_keys)
    {

        if (num_keys > 0)
        {
            time_t old_parent_max = get_node_max_key(table->pager, parent);
            *internal_node_right_child(parent) = *internal_node_child(parent, num_keys - 1);
            *internal_node_num_key(parent) = num_keys - 1;
            time_t new_parent_max = get_node_max_key(table->pager, parent);

            if (!is_root_node(parent))
            {
                update_ancestor_keys(table, parent_page_num, old_parent_max, new_parent_max);
            }
        }
        else
        {
            *internal_node_right_child(parent) = INVALID_PAGE_NUM;
        }
    }
    else
    {

        for (uint32_t i = child_idx; i < num_keys - 1; i++)
        {
            memcpy(internal_node_cell(parent, i), internal_node_cell(parent, i + 1), INTERNAL_NODE_CELL_SIZE);
        }
        *internal_node_num_key(parent) = num_keys - 1;
    }

    if (is_root_node(parent))
    {

        if (*internal_node_num_key(parent) == 0)
        {
            uint32_t only_child_page = *internal_node_right_child(parent);
            if (only_child_page != INVALID_PAGE_NUM)
            {
                void *only_child = get_page(table->pager, only_child_page);
                memcpy(parent, only_child, PAGE_SIZE);
                set_root_node(parent, true);

                if (get_node_type(parent) == NODE_INTERNAL)
                {
                    uint32_t k = *internal_node_num_key(parent);
                    for (uint32_t i = 0; i < k; i++)
                    {
                        void *ch = get_page(table->pager, *internal_node_child(parent, i));
                        *node_parent(ch) = table->root_page_num;
                    }
                    void *rch = get_page(table->pager, *internal_node_right_child(parent));
                    *node_parent(rch) = table->root_page_num;
                }
            }
        }
    }
    else
    {
        if (*internal_node_num_key(parent) < INTERNAL_NODE_MIN_KEYS)
        {
            internal_node_handle_underflow(table, parent_page_num);
        }
    }
}

static void leaf_node_handle_underflow(Table *table, uint32_t page_num)
{
    void *node = get_page(table->pager, page_num);

    if (is_root_node(node))
    {
        return;
    }

    uint32_t num_cells = *leaf_node_num_cells(node);
    if (num_cells >= LEAF_NODE_MIN_CELLS)
    {
        return;
    }

    uint32_t parent_page = *node_parent(node);
    void *parent = get_page(table->pager, parent_page);
    uint32_t child_idx = internal_node_find_child_index(parent, page_num);
    uint32_t p_keys = *internal_node_num_key(parent);

    if (child_idx > 0)
    {
        uint32_t left_sibling_page = *internal_node_child(parent, child_idx - 1);
        void *left_sibling = get_page(table->pager, left_sibling_page);
        uint32_t left_cells = *leaf_node_num_cells(left_sibling);

        if (left_cells > LEAF_NODE_MIN_CELLS)
        {

            for (uint32_t i = num_cells; i > 0; i--)
            {
                memcpy(leaf_node_cell(node, i), leaf_node_cell(node, i - 1), LEAF_NODE_CELL_SIZE);
            }

            memcpy(leaf_node_cell(node, 0), leaf_node_cell(left_sibling, left_cells - 1), LEAF_NODE_CELL_SIZE);

            *leaf_node_num_cells(left_sibling) = left_cells - 1;
            *leaf_node_num_cells(node) = num_cells + 1;

            time_t left_new_max = get_node_max_key(table->pager, left_sibling);
            *internal_node_key(parent, child_idx - 1) = left_new_max;
            return;
        }
    }

    if (child_idx < p_keys)
    {
        uint32_t right_sibling_page = *internal_node_child(parent, child_idx + 1);
        void *right_sibling = get_page(table->pager, right_sibling_page);
        uint32_t right_cells = *leaf_node_num_cells(right_sibling);

        if (right_cells > LEAF_NODE_MIN_CELLS)
        {
            time_t old_max = get_node_max_key(table->pager, node);

            memcpy(leaf_node_cell(node, num_cells), leaf_node_cell(right_sibling, 0), LEAF_NODE_CELL_SIZE);

            for (uint32_t i = 0; i < right_cells - 1; i++)
            {
                memcpy(leaf_node_cell(right_sibling, i), leaf_node_cell(right_sibling, i + 1), LEAF_NODE_CELL_SIZE);
            }

            *leaf_node_num_cells(node) = num_cells + 1;
            *leaf_node_num_cells(right_sibling) = right_cells - 1;

            time_t new_max = get_node_max_key(table->pager, node);
            if (child_idx < p_keys)
            {
                *internal_node_key(parent, child_idx) = new_max;
            }
            else
            {
                update_ancestor_keys(table, parent_page, old_max, new_max);
            }
            return;
        }
    }

    if (child_idx > 0)
    {

        uint32_t left_sibling_page = *internal_node_child(parent, child_idx - 1);
        void *left_sibling = get_page(table->pager, left_sibling_page);
        uint32_t left_cells = *leaf_node_num_cells(left_sibling);

        for (uint32_t i = 0; i < num_cells; i++)
        {
            memcpy(leaf_node_cell(left_sibling, left_cells + i), leaf_node_cell(node, i), LEAF_NODE_CELL_SIZE);
        }
        *leaf_node_num_cells(left_sibling) = left_cells + num_cells;

        *leaf_node_next_leaf(left_sibling) = *leaf_node_next_leaf(node);

        time_t left_new_max = get_node_max_key(table->pager, left_sibling);
        *internal_node_key(parent, child_idx - 1) = left_new_max;

        internal_node_remove_child(table, parent_page, page_num);
    }
    else
    {

        uint32_t right_sibling_page = *internal_node_child(parent, 1);
        void *right_sibling = get_page(table->pager, right_sibling_page);
        uint32_t right_cells = *leaf_node_num_cells(right_sibling);

        for (uint32_t i = 0; i < right_cells; i++)
        {
            memcpy(leaf_node_cell(node, num_cells + i), leaf_node_cell(right_sibling, i), LEAF_NODE_CELL_SIZE);
        }
        *leaf_node_num_cells(node) = num_cells + right_cells;

        *leaf_node_next_leaf(node) = *leaf_node_next_leaf(right_sibling);

        time_t node_new_max = get_node_max_key(table->pager, node);
        *internal_node_key(parent, 0) = node_new_max;

        internal_node_remove_child(table, parent_page, right_sibling_page);
    }
}

bool leaf_node_delete(Table *table, time_t key)
{

    Cursor *cursor = table_find(table, key);
    void *node = get_page(table->pager, cursor->page_num);
    uint32_t num_cells = *leaf_node_num_cells(node);

    if (cursor->cell_num >= num_cells || *leaf_node_key(node, cursor->cell_num) != key)
    {
        free(cursor);
        return false;
    }

    uint32_t page_num = cursor->page_num;
    uint32_t cell_num = cursor->cell_num;
    time_t old_max = get_node_max_key(table->pager, node);
    bool was_max_key = (cell_num == num_cells - 1);

    for (uint32_t i = cell_num; i < num_cells - 1; i++)
    {
        memcpy(leaf_node_cell(node, i), leaf_node_cell(node, i + 1), LEAF_NODE_CELL_SIZE);
    }
    *leaf_node_num_cells(node) -= 1;
    num_cells--;

    free(cursor);

    if (was_max_key && !is_root_node(node) && num_cells > 0)
    {
        time_t new_max = get_node_max_key(table->pager, node);
        update_ancestor_keys(table, page_num, old_max, new_max);
    }

    leaf_node_handle_underflow(table, page_num);

    return true;
}

bool btree_delete(Table *table, time_t key)
{
    return leaf_node_delete(table, key);
}

void internal_node_split_and_insert(Table *table, uint32_t parent_page_num, uint32_t child_page_num)
{
    uint32_t old_page_num = parent_page_num;
    void *old_node = get_page(table->pager, parent_page_num);
    time_t old_max = get_node_max_key(table->pager, old_node);

    void *child = get_page(table->pager, child_page_num);
    time_t child_max = get_node_max_key(table->pager, child);

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
    time_t max_after_split = get_node_max_key(table->pager, old_node);
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
