#include "node.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

NodeType get_node_type(void *node)
{
    uint32_t value = *((uint8_t *)((char *)node + NODE_TYPE_OFFSET));
    return (NodeType)value;
}

void set_node_type(void *node, NodeType type)
{
    uint8_t value = type;
    *((uint8_t *)((char *)node + NODE_TYPE_OFFSET)) = value;
}

bool is_root_node(void *node)
{
    uint8_t value = *((uint32_t *)((char *)node + IS_ROOT_OFFSET));
    return value != 0;
}

void set_root_node(void *node, bool is_root)
{
    uint8_t value = is_root;
    *((uint32_t *)((char *)node + IS_ROOT_OFFSET)) = value;
}

uint32_t *node_parent(void *node)
{
    return (uint32_t *)((char *)node + PARENT_POINTER_OFFSET);
}

time_t get_node_max_key(Pager *pager, void *node)
{
    switch (get_node_type(node))
    {
    case NODE_INTERNAL:
    {
        uint32_t right_child_page = *internal_node_right_child(node);
        if (right_child_page == INVALID_PAGE_NUM)
        {
            return 0;
        }
        void *right_child = get_page(pager, right_child_page);
        return get_node_max_key(pager, right_child);
    }
    case NODE_LEAF:
    {
        uint32_t num_cells = *leaf_node_num_cells(node);
        if (num_cells == 0)
        {
            return 0;
        }
        return *leaf_node_key(node, num_cells - 1);
    }
    }
    return 0;
}

uint32_t *leaf_node_num_cells(void *node)
{
    return (uint32_t *)((char *)node + LEAF_NODE_NUM_CELLS_OFFSET);
}

void *leaf_node_cell(void *node, uint32_t cell_num)
{
    return (char *)node + LEAF_NODE_HEADER_SIZE + cell_num * LEAF_NODE_CELL_SIZE;
}

time_t *leaf_node_key(void *node, uint32_t cell_num)
{
    return (time_t *)leaf_node_cell(node, cell_num);
}

void *leaf_node_value(void *node, uint32_t cell_num)
{
    return (char *)leaf_node_cell(node, cell_num) + LEAF_NODE_KEY_SIZE;
}

uint32_t *leaf_node_next_leaf(void *node)
{
    return (uint32_t *)((char *)node + LEAF_NODE_NEXT_LEAF_OFFSET);
}

void initialize_leaf_node(void *node)
{
    memset(node, 0, PAGE_SIZE);
    set_node_type(node, NODE_LEAF);
    set_root_node(node, false);
    *leaf_node_num_cells(node) = 0;
    *leaf_node_next_leaf(node) = 0;
}

uint32_t *internal_node_num_key(void *node)
{
    return (uint32_t *)((char *)node + INTERNAL_NODE_NUM_KEYS_OFFSET);
}

uint32_t *internal_node_right_child(void *node)
{
    return (uint32_t *)((char *)node + INTERNAL_NODE_RIGHT_CHILD_OFFSET);
}

void initialize_internal_node(void *node)
{
    set_node_type(node, NODE_INTERNAL);
    set_root_node(node, false);
    *internal_node_num_key(node) = 0;
    *internal_node_right_child(node) = INVALID_PAGE_NUM;
}

uint32_t *internal_node_cell(void *node, uint32_t cell_num)
{
    return (uint32_t *)((char *)node + INTERNAL_NODE_HEADER_SIZE + cell_num * INTERNAL_NODE_CELL_SIZE);
}

uint32_t *internal_node_child(void *node, uint32_t child_num)
{
    uint32_t num_keys = *internal_node_num_key(node);
    if (child_num > num_keys)
    {
        printf("Tried to access child_num %d > num_keys %d\n", child_num, num_keys);
        exit(EXIT_FAILURE);
    }
    else if (child_num == num_keys)
    {
        uint32_t *right_child = internal_node_right_child(node);
        if (*right_child == INVALID_PAGE_NUM)
        {
            printf("Tried to access right child of node, but was invalid page\n");
            exit(EXIT_FAILURE);
        }
        return right_child;
    }
    else
    {
        uint32_t *child = internal_node_cell(node, child_num);
        if (*child == INVALID_PAGE_NUM)
        {
            printf("Tried to access child of node, but was invalid page\n");
            exit(EXIT_FAILURE);
        }
        return child;
    }
}

time_t *internal_node_key(void *node, uint32_t key_num)
{
    return (time_t *)((char *)internal_node_cell(node, key_num) + INTERNAL_NODE_CHILD_SIZE);
}
