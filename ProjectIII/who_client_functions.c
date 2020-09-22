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
#include <arpa/inet.h>
#include "who_client_functions.h"

extern int count_line, stop_waiting;
extern pthread_cond_t condition_variable;
extern pthread_mutex_t  mutex;
extern pthread_mutex_t mutex_print;

void *Client_Thread(void* args)
{
    int error; 
    help_args_ptr argv = (help_args_ptr)args;

    // if(error = pthread_mutex_lock(&mutex_print)) 
    // {
    //     perror("Error with pthread_mutex_lock"); 
    //     exit(1);
    // }

    // // printf("This is a new thread %ld\n", pthread_self());
    
    // if(error = pthread_mutex_unlock(&mutex_print)) 
    // {
    //     perror("Error with pthread_mutex_unlock"); 
    //     exit(1);
    // }

    if(error = pthread_mutex_lock(&mutex)) 
    {
        perror("Error with pthread_mutex_lock"); 
        exit(1);
    }

    stop_waiting--;  
    //Wait until all threads have been created 
    pthread_cond_wait(&condition_variable,&mutex);

    if(error = pthread_mutex_unlock(&mutex)) 
    {
        perror("Error with pthread_mutex_unlock"); 
        exit(1);
    }

    struct sockaddr_in server_fd;
    struct sockaddr *serverptr = (struct sockaddr *)&(server_fd);
    int Socket;

    while(1)
    {
        if(error = pthread_mutex_lock(&mutex)) 
        {
            perror("Error with pthread_mutex_lock"); 
            exit(1);
        }

        if(count_line >= ((argv->num_of_queries)))
        {  
            
            if(error = pthread_mutex_unlock(&mutex)) 
            {
                perror("Error with pthread_mutex_unlock"); 
                exit(1);
            }
            
            break;
        }

        //Take the line of each query
        if((argv->queries_array[count_line][(strlen((argv->queries_array)[count_line]))-1]) == '\n')
            argv->queries_array[count_line][(strlen((argv->queries_array)[count_line]))-1] = '\0';
        
        size_t size_array;
        int j;

        size_array = (strlen((argv->queries_array)[count_line])+1)/sizeof((argv->queries_array)[count_line][0]);
        
        char* query = (char*)malloc((strlen(argv->queries_array[count_line])+1)*sizeof(char));
        strcpy(query,argv->queries_array[count_line]);
        
        count_line++;

        if(error = pthread_mutex_unlock(&mutex)) 
        {
            perror("Error with pthread_mutex_unlock"); 
            exit(1);
        }

        //Create a TCP socket that client writes a query to the server.
        if((Socket = socket(AF_INET , SOCK_STREAM , 0)) < 0)
            perror("Error with creation of socket");
        
        server_fd.sin_family = AF_INET ;
        server_fd.sin_addr.s_addr = inet_addr(argv->servIP);
        (server_fd).sin_port = htons(argv->servPort);

        //Connect to server's query port
        if(connect(Socket, serverptr, sizeof(server_fd)) < 0)
            perror("Error with connection");
        
        // printf("Connecting to %s port %d %d.\n", argv->servIP, argv->servPort, count_line);

        //Write to server the query 
        for(j=0;j<(size_array);j++)
        {
            if(write(Socket, &query[j], 1) < 0)
            {
                perror("Error with write");
                exit(1);
            }
        }

        //Read from server the answer of the query
        int bytes;
        char* respond_message;

        bytes = Read_Socket(Socket,&respond_message);

        //Print the query with its answer
        Printf_With_Mutexes(query,respond_message);

        free(query);
        free(respond_message);
        close(Socket);
    }
}

//Read byte by byte
int Read_Socket(int socket_fd, char** respond_message)
{
    char buf[1];
        strcpy(buf,"");

        int k=0,bytes;
        if((k=read(socket_fd, buf, 1))<0)
        {
            perror("Error with read");
            exit(1);
        }
        bytes=k;

        (*respond_message) = (char *)malloc((k+1)*sizeof(char));
        strcpy((*respond_message),"");  //Initialize the array with the final message.


        while(k!=0)
        {
            if(buf[0] == '\0')  break;
            strncat((*respond_message),buf,k);
            if((k=read(socket_fd, buf, 1))<0)
            {
                perror("Error with read");
                exit(1);
            }
            if(k!=0)
            { 
                bytes+=k;
                (*respond_message) = (char *)realloc((*respond_message), (bytes+k+1)*sizeof(char));
            }
        }
        strncat((*respond_message),"\0",1);  //Put the null character in the end of message.
        
        return bytes;
}

//Write byte by byte
void Write_Socket(int socket_fd, char* message)
{
    size_t size_array;
    int j;
    size_array = (strlen(message)+1)/sizeof(message[0]);

    for(j=0;j<(size_array);j++)
    {
        if(write(socket_fd, &message[j], 1) < 0)
        {
            perror("Error with write");
            exit(1);
        }
    }
}

void Printf_With_Mutexes(char* query, char* answer)
{
    int error;

    if(error = pthread_mutex_lock(&mutex_print)) 
    {
        perror("Error with pthread_mutex_lock"); 
        exit(1);
    }

    printf("%s\n", query);
    printf("%s\n", answer);

    if(error = pthread_mutex_unlock(&mutex_print)) 
    {
        perror("Error with pthread_mutex_unlock"); 
        exit(1);
    }
}