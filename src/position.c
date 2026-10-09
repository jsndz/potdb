#include "position.h"
#include "repl.h"
#include <stdlib.h>

Position *get_position(InputBuffer *inputbuffer)
{
    Position *pos = malloc(sizeof(Position));
    if (pos) {
        pos->x = 0;
        pos->y = 0;
        if (inputbuffer && inputbuffer->buffer) {
            sscanf(inputbuffer->buffer, "insert %d %d", &pos->x, &pos->y);
        }
    }
    return pos;
}
