#ifndef ROW_H
#define ROW_H

#include <stdint.h>
#include <time.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "position.h"

#define COL_USERNAME_SIZE sizeof(time_t)
#define COL_EMAIL_SIZE sizeof(Position)

#define size_of_attribute(Struct, Attribute) sizeof(((Struct *)0)->Attribute)


typedef struct
{
    time_t timestamp;
    Position pos;
} Row;

#define TIMESTAMP_SIZE sizeof(time_t)
#define POSITION_SIZE sizeof(Position)

#define TIMESTAMP_OFFSET ((time_t)0)
#define POSITION_OFFSET (TIMESTAMP_OFFSET + TIMESTAMP_SIZE)

#define ROW_SIZE (TIMESTAMP_SIZE + POSITION_SIZE)

void serialize_row(Row *source, void *destination);
void deserialize_row(void *source, Row *destination);
void print_row(Row *row);

#endif // ROW_H
