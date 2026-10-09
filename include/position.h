#ifndef POSITION_H
#define POSITION_H

typedef struct InputBuffer InputBuffer;

typedef struct 
{
    int x;
    int y;
} Position;

Position *get_position(InputBuffer *inputbuffer);

#endif 
