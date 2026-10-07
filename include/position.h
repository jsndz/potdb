#ifndef POSITION_H
#define POSITION_H
#include <repl.h>

typedef struct 
{
    int x;
    int y;
} Position;

Position *get_position(InputBuffer *inputbuffer){
    Position *pos = malloc(sizeof(Position));
    return pos;
}

#endif 
