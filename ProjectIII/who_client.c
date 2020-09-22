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
#include "who_client_functions.h"

#define PERMS 0666

volatile sig_atomic_t flag=0;
// volatile sig_atomic_t worker_pid=-10;

int count_line;
int stop_waiting;  //Initiallized by numThreads and helps me to wait the creation of all client threads before I begin any connection with server
pthread_cond_t condition_variable;
pthread_mutex_t  mutex;
pthread_mutex_t mutex_print;

int main(int argc, char** argv)
{
    //Check for invalid input.
    if( argc < 9 || argc > 9 )
    {
        printf("Invalid Input. Please give 9 arguments.\n");
        return(1);
    }
    
    int i;
    char* queryFile;
    
    help_args_ptr args = (help_args_ptr)malloc(sizeof(struct Helping_Arguments));
    
    //In case we give the arguments with random sequence.
    for(i=1;i<=7;i+=2)
    {
        if(strcmp(argv[i],"-q") == 0)
        {
            queryFile = (char*)malloc((strlen(argv[i+1])+1)*sizeof(char));
            strcpy(queryFile,argv[i+1]);
        }
        else if(strcmp(argv[i],"-sip") == 0)
        {
            args->servIP = (char*)malloc((strlen(argv[i+1])+1)*sizeof(char));
            strcpy(args->servIP,argv[i+1]);
        }
        else if(strcmp(argv[i],"-w") == 0)
            args->numThreads = atoi(argv[i+1]);
        else if(strcmp(argv[i],"-sp") == 0)
            args->servPort = atoi(argv[i+1]);
        else
        {
            printf("Please give an input with this form: ./diseaseAggregator –w numWorkers -b bufferSize -i input_dir\n");
            free(args);
            exit(1);
        }
    }

    //Check for validation of num of Threads.
    if(args->servPort <= 0)
    {
        printf("Please give at least 1 in buffersize/numWorkers\n");
        free(queryFile);
        free(args);
        exit(1);
    }
    
    struct dirent *ent;

    FILE* fp;
    char buf[1024];
    if ((fp = fopen(queryFile, "r")) == NULL)
    { 
        perror("Can't open the file.");
        return 1;
    }

    args->num_of_queries=0;

    while (fgets(buf, sizeof(buf), fp) != NULL)
    {
        (args->num_of_queries)++;
    }
    fclose(fp);

    if ((fp = fopen(queryFile, "r")) == NULL)
    { 
        perror("Can't open the file");
        return 1;
    }

    if((args->num_of_queries) != 0)
    {
        args->queries_array = (char**)malloc((args->num_of_queries)*(sizeof(char*)));
        i=0;

        while (fgets(buf, sizeof(buf), fp) != NULL)
        {
            (args->queries_array)[i] = (char*)malloc((strlen(buf)+1)*sizeof(char));
            strcpy((args->queries_array)[i],buf);
            i++;
        }
    }
    else
    {
        printf("Query File is empty. PLease give a non-empty file!\n");
        free(queryFile);
        free(args->servIP);
        free(args);
        exit(1);
    }
    
    fclose(fp);

    //Initiallize the contitional variables and the mutexes.
    pthread_t threads[args->numThreads];
    pthread_cond_init(&condition_variable, NULL);
    pthread_mutex_init(&mutex,NULL);
    pthread_mutex_init(&mutex_print,NULL);

    int error, status;
    count_line = 0;
    stop_waiting = args->numThreads;

    //Create numThreads Threads
    for(i=0;i<(args->numThreads);i++)
    {
        if(error = pthread_create(&(threads[i]), NULL, Client_Thread, args)) 
        {
            perror("Error with Create Thread.\n");
            exit(1);
        }
    }

    //Wait until all threads have been created   
    while(stop_waiting != 0)
    {
        // printf("%d\n",stop_waiting);
    }

    if(error = pthread_mutex_lock(&mutex)) 
    {
        perror("Error with pthread_mutex_lock"); 
        exit(1);
    }

    //When all client's threads has been created the 
    //main client thread send a signal to the client's 
    //threads to start the connection with the server.
    pthread_cond_broadcast(&condition_variable);
    
    if(error = pthread_mutex_unlock(&mutex)) 
    {
        perror("Error with pthread_mutex_unlock"); 
        exit(1);
    }

    //Wait until all client's threads have finished.
    for(i=0;i<(args->numThreads);i++)
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
    
    if(error = pthread_cond_destroy(&condition_variable)) 
    {
        perror("Error with pthread_cond_destroy"); 
        exit(1); 
    }
    

    free(queryFile);
    free(args->servIP);

    for(i=0;i<(args->num_of_queries);i++)
        free((args->queries_array)[i]);
    free(args->queries_array);
    free(args);
    
    pthread_exit(NULL);
    
    return 0;
}
    