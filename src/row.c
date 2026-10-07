#include "row.h"
#include <stdio.h>
#include <string.h>

void serialize_row(Row *source, void *destination)
{
    memcpy((char *)destination + TIMESTAMP_OFFSET, &(source->timestamp), TIMESTAMP_SIZE);
    memcpy((char *)destination + POSITION_OFFSET, &(source->timestamp), POSITION_SIZE);
}

void deserialize_row(void *source, Row *destination)
{
    memcpy(&(destination->timestamp), ((char *)source + TIMESTAMP_OFFSET), TIMESTAMP_SIZE);
    memcpy(&(destination->pos), ((char *)source + POSITION_OFFSET), POSITION_SIZE);
}

void print_row(Row *row)
{
    printf("(%f, %d, %d)\n", row->timestamp, row->pos.x , row->pos.y);
}
