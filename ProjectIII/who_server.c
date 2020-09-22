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
#include "who_server_functions.h"
#include "buffer.h"
#include "handler_server.h"

#define PERMS 0666

volatile sig_atomic_t flag_server=0;
volatile sig_atomic_t worker_pid=-10;

pthread_cond_t condition_variable;
pthread_mutex_t  mutex;
pthread_mutex_t  mutex_print;
pthread_mutex_t  mutex_thread_worker;
pthread_mutex_t  mutex_statistics;
pthread_cond_t condition_non_empty;
pthread_cond_t condition_non_full;
pthread_cond_t condition_thread_finished_with_worker;
pthread_cond_t condition_thread_finished_with_statistics;
struct Buffer buffer;
int stop_waiting;
extern int num_workers;
char* workerIP=NULL;

int main(int argc, char** argv)
{
    //Check for invalid input.
    if( argc < 9 || argc > 9 )
    {
        printf("Invalid Input. Please give 7 arguments.\n");
        return(1);
    }
    
    int  numThreads, bufferSize, i, queryPortNum, statisticsPortNum;
    help_args_ptr args = (help_args_ptr)malloc(sizeof(struct Helping_Arguments));

    //In case we give the arguments with random sequence.
    for(i=1;i<=7;i+=2)
    {
        if(strcmp(argv[i],"-q") == 0)
            queryPortNum = atoi(argv[i+1]);
        else if(strcmp(argv[i],"-s") == 0)
            statisticsPortNum = atoi(argv[i+1]);
        else if(strcmp(argv[i],"-w") == 0)
            numThreads = atoi(argv[i+1]);
        else if(strcmp(argv[i],"-b") == 0)
            (args->bufferSize) = atoi(argv[i+1]);
        else
        {
            printf("Please give an input with this form: ./diseaseAggregator –w numWorkers -b bufferSize -i input_dir\n");
            exit(1);
        }
    }
    stop_waiting = numThreads;

    //Check for validation of bufferSize and num of Threads.
    if((args->bufferSize==0) || (numThreads==0))
    {
        printf("Please give at least 1 in buffersize/numThreads\n");
        free(args);
        exit(1);
    }

    //For signal SIGINT
    struct sigaction act;
 
	memset(&act, 0, sizeof(act));
	act.sa_sigaction = Handler_Server;
	act.sa_flags = SA_SIGINFO;
 
	if (sigaction(SIGINT, &act, NULL) < 0) {
		perror ("Sigaction");
		return 1;
	}

    //Initiallize all the condition variables and the mutexes.
    pthread_t threads[numThreads];
    pthread_cond_init(&condition_variable, NULL);
    pthread_mutex_init(&mutex,NULL);
    pthread_mutex_init(&mutex_print,NULL);
    pthread_mutex_init(&mutex_thread_worker,NULL);
    pthread_mutex_init(&mutex_statistics,NULL);
    pthread_cond_init(&condition_non_empty, NULL);
    pthread_cond_init(&condition_non_full, NULL);
    pthread_cond_init(&condition_thread_finished_with_statistics, NULL);
    pthread_cond_init(&condition_thread_finished_with_worker, NULL);
    
    //Initialize the struct Buffer with the given bufferSize.
    Initialize_Buffer(args->bufferSize);

    //Take the server IP address
    // (args->servIP) = (char*)malloc((strlen("127.0.0.1")+1)*sizeof(char));
    // strcpy(args->servIP,"127.0.0.1");

    Get_ServIP(&args->servIP);

    int error, status;

    //Create numTreads Threads and pss through the arguments the bufferSize and the servIP.
    for(i=0;i<(numThreads);i++)
    {
        if(error = pthread_create(&(threads[i]), NULL, Server_Thread, args)) 
        {
            perror("Error with Create Thread");
            exit(1);
        }
    }

    struct sockaddr_in worker_server_fd;
    socklen_t workerlen = sizeof(worker_server_fd);
    int worker_server_socket, new_worker_server_socket;

    struct sockaddr *workerptr = (struct sockaddr *)&worker_server_fd;

    //Create a TCP socket that listens for worker's connections
    if((worker_server_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        perror("Error with creation of the socket");
    
    worker_server_fd.sin_family = AF_INET ;
    worker_server_fd.sin_addr.s_addr = inet_addr(args->servIP);
    worker_server_fd.sin_port = htons(statisticsPortNum);
    int one = 1;

    //Make port reusable.
    if(setsockopt(worker_server_socket, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int)) < 0)  
    { 
        perror("Error with setsockopt(SO_REUSEADDR)");
        exit(1);
    }

    if(bind(worker_server_socket, workerptr, workerlen) < 0)
    {
        perror("Error with bind");
        exit(1);
    }
    
    if(listen(worker_server_socket, 1000) < 0)
    {
        perror("Error with listen");
        exit(1);
    }

    if(error = pthread_mutex_lock(&mutex_print)) 
    {
        perror("Error with pthread_mutex_lock"); 
        exit(1);
    }

    printf("Listening for worker's connections to port %d to IP %s\n", statisticsPortNum, args->servIP);
   
    if(error = pthread_mutex_unlock(&mutex_print)) 
    {
        perror("Error with pthread_mutex_unlock"); 
        exit(1);
    }


    while(1)
    {
        //Accept worker's connections
        if((new_worker_server_socket = accept(worker_server_socket, workerptr, &workerlen)) < 0)
        { 
            // if(flag == 1)
            // {      
            //     pthread_cond_broadcast(&condition_non_empty);
            //     for(int j=0;j<buffer.count;j++)le
            //         close(buffer.data[j]);
            //     break;
            // }
            perror("Error with accept");
            exit(1);
        }

        if(workerIP == NULL)
        {
            workerIP = (char*)malloc((strlen(inet_ntoa((((struct sockaddr_in*)workerptr)->sin_addr)))+1)*sizeof(char));
            strcpy(workerIP,inet_ntoa((((struct sockaddr_in*)workerptr)->sin_addr)));
        }


        // if(error = pthread_mutex_lock(&mutex_print)) 
        // {
        //     perror("Error with pthread_mutex_lock"); 
        //     exit(1);
        // }

        // printf("Accept connection with IP %s\n", workerIP);
        
        // if(error = pthread_mutex_unlock(&mutex_print)) 
        // {
        //     perror("Error with pthread_mutex_unlock"); 
        //     exit(1);
        // }


        if(error = pthread_mutex_lock(&mutex_thread_worker)) 
        {
            perror("Error with pthread_mutex_lock"); 
            exit(1);
        }
        
        //Insert to buffer the socket file descriptor which just accept.
        Insert_to_Buffer(new_worker_server_socket, args->bufferSize);
        
        //Main Server wait for one thread to finish the 
        //information of worker and the printing of statistics.
        pthread_cond_wait(&condition_thread_finished_with_worker,&mutex_thread_worker);

        if(error = pthread_mutex_unlock(&mutex_thread_worker)) 
        {
            perror("Error with pthread_mutex_unlock"); 
            exit(1);
        }

        //In case workers are over, then I send signal to 
        //all server threads, in order to unblock from 
        //pthread_cond_wait() of function Extract_from_Buffer()
        if(num_workers == -10)
        {
            pthread_cond_broadcast(&condition_non_empty);
            for(int j=0;j<buffer.count;j++)
                close(buffer.data[j]);
            break;
        }
    }

    //Wait until all server threads 
    //have finished with Statistics.
    while(stop_waiting != 0)
    {
        // printf("%d\n",stop_waiting);
    }

    num_workers = -100;  //Change the value of num_workers so as to won't stuck in Extract_from_buffer() to the future.

    // printf("Server threads can start now.\n");
    
    //Now that all server threads have finished with the statistics 
    //send signal to start with the connection with the client.
    pthread_cond_broadcast(&condition_thread_finished_with_statistics);
    
    //////////////////////////////////////////////////////////////////////
    
    struct sockaddr_in server_fd, client_fd;
    socklen_t clientlen = sizeof(client_fd);
    int Socket_Client, new_socket_client;

    struct sockaddr *serverptr = (struct sockaddr *)&server_fd;
    struct sockaddr *clientptr = (struct sockaddr *)&client_fd;

    //Create a TCP socket that listens for client's connections
    if((Socket_Client = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        perror("Error with creation of the socket");
    
    server_fd.sin_family = AF_INET ;
    server_fd.sin_addr.s_addr = inet_addr(args->servIP);
    server_fd.sin_port = htons(queryPortNum);

    //Make port reusable.
    if(setsockopt(Socket_Client, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int)) < 0)  
    { 
        perror("Error with setsockopt(SO_REUSEADDR)");
        exit(1);
    }

    if(bind(Socket_Client, serverptr, sizeof(server_fd)) < 0)
    {
        perror("Error with bind");
        exit(1);
    }
    
    if(listen(Socket_Client, 1000) < 0)
    {
        perror("Error with listen");
        exit(1);
    }

    if(error = pthread_mutex_lock(&mutex_print)) 
    {
        perror("Error with pthread_mutex_lock"); 
        exit(1);
    }

    printf("Listening for client's connections to port %d to IP %s\n", queryPortNum, args->servIP);
    
    if(error = pthread_mutex_unlock(&mutex_print)) 
    {
        perror("Error with pthread_mutex_unlock"); 
        exit(1);
    }


    while(1)
    {
        //Accept client's connections. 
        if((new_socket_client = accept(Socket_Client, clientptr, &clientlen)) < 0)
        {
            //In case server accept SIGINT/SIGQUIT 
            //signal to terminate the programm.
            if(flag_server == 1)
            {      
                //Send signal to unblock the pthread_cond_wait of Extract_to_Buffer()
                pthread_cond_broadcast(&condition_non_empty);
                for(int j=0;j<(buffer.count);j++)
                    close(buffer.data[j]);
                break;
            }
            perror("Error with accept");
            exit(1);
        }

        // if(error = pthread_mutex_lock(&mutex_print)) 
        // {
        //     perror("Error with pthread_mutex_lock"); 
        //     exit(1);
        // }
      
        // printf("Connection accepted.\n");

        // if(error = pthread_mutex_unlock(&mutex_print)) 
        // {
        //     perror("Error with pthread_mutex_unlock"); 
        //     exit(1);
        // }
        

        //Insert to the Buffer the file descriptor of client's connection.
        Insert_to_Buffer(new_socket_client, args->bufferSize);
    }

    //Wait until all threads have finished.
    for(i=0;i<(numThreads);i++)
    {
        if(error = pthread_join(threads[i], NULL))
        {
            perror("Error with thread join");
            exit(1);
        }
    }

    //Deallocate the memory and destroy all 
    //the mutexes and condition variables.
    if(error = pthread_mutex_destroy (&mutex))
    {
        perror("Error with pthread_mutex_destroy"); 
        exit(1); 
    }

    if(error = pthread_mutex_destroy (&mutex_print))
    {
        perror("Error with pthread_mutex_destroy"); 
        exit(1); 
    }

    if(error = pthread_mutex_destroy (&mutex_statistics))
    {
        perror("Error with pthread_mutex_destroy"); 
        exit(1); 
    }

    if(error = pthread_mutex_destroy (&mutex_thread_worker))
    {
        perror("Error with pthread_mutex_destroy"); 
        exit(1); 
    }
    
    if(error = pthread_cond_destroy(&condition_variable)) 
    {
        perror("Error with pthread_cond_destroy"); 
        exit(1); 
    }
    
    if(error = pthread_cond_destroy(&condition_non_empty)) 
    {
        perror("Error with pthread_cond_destroy"); 
        exit(1); 
    }

    if(error = pthread_cond_destroy(&condition_non_full)) 
    {
        perror("Error with pthread_cond_destroy"); 
        exit(1); 
    }

    if(error = pthread_cond_destroy(&condition_thread_finished_with_statistics)) 
    {
        perror("Error with pthread_cond_destroy"); 
        exit(1); 
    }

    if(error = pthread_cond_destroy(&condition_thread_finished_with_worker)) 
    {
        perror("Error with pthread_cond_destroy"); 
        exit(1); 
    }

    //Close the sockets that listens to the client's and worker's connections.
    close(Socket_Client);
    close(worker_server_socket);

    //Deallocate the memory

    if(args->servIP != NULL)  free(args->servIP);
    if(args != NULL)  free(args);
    if(workerIP != NULL)  free(workerIP);
    if(buffer.data != NULL)  free(buffer.data);

    return 0;
}   
    