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
    int numThreads;
    int Socket;
    int servPort;
    int num_of_queries;
    char* servIP;
    char** queries_array;
    // struct sockaddr *serverptr;
    // struct sockaddr_in server_fd;
};

typedef struct Helping_Arguments *help_args_ptr;

void *Client_Thread();
int Read_Socket(int, char**);
void Write_Socket(int, char*);
void Printf_With_Mutexes(char*, char*);