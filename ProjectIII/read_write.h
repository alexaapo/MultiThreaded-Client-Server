#ifndef READ_WRITE_H
#define READ_WRITE_H

#include "bucket.h"
#include "hash.h"

void Write_Pipe(char*,char*, int);
char *Read_Pipe(char**, char*, int);
int Read_Socket(int, char**);
void Write_Socket(int, char*);
int Worker_Update_Structures(bucket_ptr*, bucket_ptr*, rec_ptr*, int, int, char***, char*, char***);
void Get_ServIP(char**);

#endif