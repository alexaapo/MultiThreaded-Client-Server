#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <ctype.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <signal.h>

struct Helping_Arguments
{
    int bufferSize;
    // int Socket;
    char* servIP;
};

typedef struct Helping_Arguments *help_args_ptr;

void *Server_Thread(void*);
void Insert_to_Buffer(int, int);
int Extract_from_Buffer(int);
int Read_Socket(int, char**);
void Write_Socket(int, char*);
void Server_Worker_Communicate(int, char*, char**);
void Print_Statistics_With_Mutexes(char*);
void Printf_With_Mutexes(char*, char*);
void Answer_from_Workers(char*, char*, char**);
void Get_ServIP(char**);