#ifndef NODE_H
#define NODE_H

#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "row.h"
#include "pager.h"

#define INVALID_PAGE_NUM UINT32_MAX

typedef enum
{
    NODE_INTERNAL,
    NODE_LEAF
} NodeType;

#define NODE_TYPE_SIZE ((uint32_t)sizeof(uint8_t))
#define NODE_TYPE_OFFSET ((uint32_t)0)
#define IS_ROOT_SIZE ((uint32_t)sizeof(uint8_t))
#define IS_ROOT_OFFSET (NODE_TYPE_SIZE)
#define PARENT_POINTER_SIZE ((uint32_t)sizeof(uint32_t))
#define PARENT_POINTER_OFFSET (IS_ROOT_OFFSET + IS_ROOT_SIZE)
#define COMMON_NODE_HEADER_SIZE (NODE_TYPE_SIZE + IS_ROOT_SIZE + PARENT_POINTER_SIZE)

#define INTERNAL_NODE_NUM_KEYS_SIZE ((uint32_t)sizeof(uint32_t))
#define INTERNAL_NODE_NUM_KEYS_OFFSET (COMMON_NODE_HEADER_SIZE)
#define INTERNAL_NODE_RIGHT_CHILD_SIZE ((uint32_t)sizeof(uint32_t))
#define INTERNAL_NODE_RIGHT_CHILD_OFFSET (INTERNAL_NODE_NUM_KEYS_OFFSET + INTERNAL_NODE_NUM_KEYS_SIZE)
#define INTERNAL_NODE_HEADER_SIZE (COMMON_NODE_HEADER_SIZE + INTERNAL_NODE_NUM_KEYS_SIZE + INTERNAL_NODE_RIGHT_CHILD_SIZE)

#define INTERNAL_NODE_KEY_SIZE ((uint32_t)sizeof(time_t))
#define INTERNAL_NODE_CHILD_SIZE ((uint32_t)sizeof(uint32_t))
#define INTERNAL_NODE_CELL_SIZE (INTERNAL_NODE_CHILD_SIZE + INTERNAL_NODE_KEY_SIZE)
#define INTERNAL_NODE_MAX_KEYS 3

#define LEAF_NODE_NUM_CELLS_SIZE ((uint32_t)sizeof(uint32_t))
#define LEAF_NODE_NUM_CELLS_OFFSET (COMMON_NODE_HEADER_SIZE)
#define LEAF_NODE_NEXT_LEAF_SIZE ((uint32_t)sizeof(uint32_t))
#define LEAF_NODE_NEXT_LEAF_OFFSET (LEAF_NODE_NUM_CELLS_OFFSET + LEAF_NODE_NUM_CELLS_SIZE)
#define LEAF_NODE_HEADER_SIZE (COMMON_NODE_HEADER_SIZE + LEAF_NODE_NUM_CELLS_SIZE + LEAF_NODE_NEXT_LEAF_SIZE)

#define LEAF_NODE_KEY_SIZE ((uint32_t)sizeof(time_t))
#define LEAF_NODE_KEY_OFFSET 0
#define LEAF_NODE_VALUE_SIZE (ROW_SIZE)
#define LEAF_NODE_VALUE_OFFSET (LEAF_NODE_KEY_OFFSET + LEAF_NODE_KEY_SIZE)
#define LEAF_NODE_CELL_SIZE (LEAF_NODE_KEY_SIZE + LEAF_NODE_VALUE_SIZE)
#define LEAF_NODE_SPACE_FOR_CELLS (PAGE_SIZE - LEAF_NODE_HEADER_SIZE)
#define LEAF_NODE_MAX_CELLS (LEAF_NODE_SPACE_FOR_CELLS / LEAF_NODE_CELL_SIZE)

// General Node Functions
NodeType get_node_type(void *node);
void set_node_type(void *node, NodeType type);
bool is_root_node(void *node);
void set_root_node(void *node, bool is_root);
uint32_t *node_parent(void *node);
time_t get_node_max_key(Pager *pager, void *node);

// Leaf Node Accessors
uint32_t *leaf_node_num_cells(void *node);
void *leaf_node_cell(void *node, uint32_t cell_num);
time_t *leaf_node_key(void *node, uint32_t cell_num);
void *leaf_node_value(void *node, uint32_t cell_num);
uint32_t *leaf_node_next_leaf(void *node);
void initialize_leaf_node(void *node);

// Internal Node Accessors
uint32_t *internal_node_num_key(void *node);
uint32_t *internal_node_right_child(void *node);
uint32_t *internal_node_cell(void *node, uint32_t cell_num);
uint32_t *internal_node_child(void *node, uint32_t child_num);
time_t *internal_node_key(void *node, uint32_t key_num);
void initialize_internal_node(void *node);

#endif // NODE_H
