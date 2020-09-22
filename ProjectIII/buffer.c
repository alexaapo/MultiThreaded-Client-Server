#include <stdlib.h>
#include "buffer.h"

extern struct Buffer buffer;

void Initialize_Buffer(int bufferSize)
{
    buffer.data = (int *)malloc(bufferSize*sizeof(int));
    buffer.start = 0;
    buffer.end = -1;
    buffer.count = 0;
}

