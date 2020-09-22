struct Buffer
{
    int* data;
    int start;
    int end;
    int count;
};

typedef struct Buffer *bufferptr;

void Initialize_Buffer(int);