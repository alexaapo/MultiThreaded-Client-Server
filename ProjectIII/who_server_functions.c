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
#include <netinet/in.h>
#include <arpa/inet.h>
#include "who_server_functions.h"
#include "buffer.h"

extern volatile sig_atomic_t flag_server;
// extern volatile sig_atomic_t worker_pid;

extern pthread_cond_t condition_variable;
extern pthread_mutex_t mutex;
extern pthread_mutex_t mutex_print;
extern pthread_mutex_t  mutex_thread_worker;
extern pthread_mutex_t  mutex_statistics;
extern pthread_cond_t condition_non_empty;
extern pthread_cond_t condition_non_full;
extern pthread_cond_t condition_thread_finished_with_worker;
extern pthread_cond_t condition_thread_finished_with_statistics;
extern struct Buffer buffer;
extern int stop_waiting;  //Global variable that be initiallized with num_of_server_threads to check if all threads have finished with the statistics.
int num_workers=0;
int* worker_ports=NULL;  //An array of integers that saves the ports of each worker. 
int count_worker_ports=0; //(Or count_of_workers) For helping me to traverse the above array
int deleted=0;  //This global variable helped me to deallocate one time (one thread) the array of workers ports
extern char* workerIP;

void *Server_Thread(void* args)
{
    help_args_ptr argv = (help_args_ptr)args;
    int socket_fd,error;

    // if(error = pthread_mutex_lock(&mutex_print)) 
    // {
    //     perror("Error with pthread_mutex_lock"); 
    //     exit(1);
    // }

    // printf("This is a new thread %ld\n", pthread_self());
    
    // if(error = pthread_mutex_unlock(&mutex_print)) 
    // {
    //     perror("Error with pthread_mutex_unlock"); 
    //     exit(1);
    // }

    char* statistics;
    char num_of_workers[30], workerPort[30];

    //This while is for Connections with the ports of each Worker.
    while(1)
    {
        //Extract from buffer a socket file descriptor
        socket_fd = Extract_from_Buffer(argv->bufferSize);

        if(socket_fd == -1)  break;  //In case main server thread have be received a SIGINT/SIGQUIT signal
        if(socket_fd == -2)  break;  //In case all server threads have finished with the statistics  

        //Read from worker's socket descriptor all the 
        //informations that is useful for a server thread.
        char* respond_message;
        int bytes;

        bytes = Read_Socket(socket_fd,&respond_message);
        
        int i=0,j=0, num_of_directories, line=0;
        char temp[bytes], input_dir[20];
        strcpy(temp,"");

        //Now I have to translate the message that the worker send,
        //according to the communication protocol, so to take the 
        //appropriate information the thread needs.
        while(respond_message[i] != '\0')
        {
            while((respond_message[i] != '\n') && (respond_message[i] != '\0'))
            {            
                //1st line: number of workers.
                //2nd line: worker port number.
                //3rd line: statistics.
                if(line != 2)  strncat(temp,&respond_message[i],1);
                else
                {
                    while((respond_message[i] != '\0'))
                    {
                        strncat(temp,&respond_message[i],1);
                        i++;
                    }
                    statistics = (char*)malloc((strlen(temp)+1)*sizeof(char));
                    strcpy(statistics,temp);

                    //When the thread take the statistics from Worker 
                    //do the appropriate Print of them with Mutexes.
                    Print_Statistics_With_Mutexes(statistics);

                    strcpy(temp,"");
                }
                
                if(respond_message[i] != '\0')  i++;
            }
            if(line == 0)
            {
                strcpy(num_of_workers,temp);
            }
            if(line == 1)
            {  
                strcpy(workerPort,temp);

                if(error = pthread_mutex_lock(&mutex)) 
                {
                    perror("Error with pthread_mutex_lock"); 
                    exit(1);
                }

                //Save to the global array the worker's port
                if(worker_ports == NULL)
                {
                    worker_ports = (int*)malloc(sizeof(int));
                    worker_ports[count_worker_ports] = atoi(workerPort);
                }
                else
                {
                    worker_ports = (int*)realloc(worker_ports, (count_worker_ports+1)*(sizeof(int)));
                    worker_ports[count_worker_ports] = atoi(workerPort);
                }

                count_worker_ports++; //Increase the count of worker ports, so in the end we know exactly the number of total worker ports.         

                if(error = pthread_mutex_unlock(&mutex)) 
                {
                    perror("Error with pthread_mutex_unlock"); 
                    exit(1);
                }
            }
            line++;
            strcpy(temp,"");
            if(respond_message[i] != '\0')  i++;
        }

        free(respond_message);
        free(statistics);

        close(socket_fd);  //Close the socket of a specific worker.

        if(error = pthread_mutex_lock(&mutex)) 
        {
            perror("Error with pthread_mutex_lock"); 
            exit(1);
        }
       
        num_workers++;

        //If workers are over we can exit the while() of connection with Workers
        if(num_workers == atoi(num_of_workers))
        {  
            
            if(error = pthread_mutex_unlock(&mutex)) 
            {
                perror("Error with pthread_mutex_unlock"); 
                exit(1);
            }

            //Change the value of the global variable, 
            //in order to main server thread understand 
            //when the last thread have finished with 
            //the worker's information.
            num_workers=-10;  

            if(error = pthread_mutex_lock(&mutex_thread_worker)) 
            {
                perror("Error with pthread_mutex_lock"); 
                exit(1);
            }

            pthread_cond_signal(&condition_thread_finished_with_worker);

            if(error = pthread_mutex_unlock(&mutex_thread_worker)) 
            {
                perror("Error with pthread_mutex_unlock"); 
                exit(1);
            }
            
           
            break;
        }
        
        if(error = pthread_mutex_unlock(&mutex)) 
        {
            perror("Error with pthread_mutex_unlock"); 
            exit(1);
        }

        if(error = pthread_mutex_lock(&mutex_thread_worker)) 
        {
            perror("Error with pthread_mutex_lock"); 
            exit(1);
        }

        //If a thread have finished with the worker's information and statistics
        // send a signal to the main server thread for waiting (until all threads have finished).
        pthread_cond_signal(&condition_thread_finished_with_worker);

        if(error = pthread_mutex_unlock(&mutex_thread_worker)) 
        {
            perror("Error with pthread_mutex_unlock"); 
            exit(1);
        }
    }

    if(error = pthread_mutex_lock(&mutex_statistics)) 
    {
        perror("Error with pthread_mutex_lock"); 
        exit(1);
    }

    //Decrease the global valiable (that has been initiallized with the num_of_server_threads), 
    //so as to tell to the main server thread to send a signal to begin the client connection
    // when all server threads have finished with the statistics of each worker.
    stop_waiting--;  

    //Wait until all server threads have finished with the statistics.
    pthread_cond_wait(&condition_thread_finished_with_statistics,&mutex_statistics);

    if(error = pthread_mutex_unlock(&mutex_statistics)) 
    {
        perror("Error with pthread_mutex_unlock"); 
        exit(1);
    }

    //This while() is for the connections of the client with 
    //server to send him the query and then server haw to send 
    //the query to the worker to take the answer and send it back to the client.
    while(1)
    {
        //Extract from buffer the socket descriptor of the client
        socket_fd = Extract_from_Buffer(argv->bufferSize);
        
        if(socket_fd == -1)  break;  //In case main server thread have be received a SIGINT/SIGQUIT signal
        
        char* query;
        int Bytes;

        //Read the query of the client
        Bytes = Read_Socket(socket_fd,&query);
        
        char* answer_of_query=NULL;

        //Connect to each worker to find the answer to the query of worker.
        Answer_from_Workers(query, argv->servIP, &answer_of_query);

        //Write the answer of client's query to the client.
        Write_Socket(socket_fd, answer_of_query);
        
        //Server prints the query with the answer.
        Printf_With_Mutexes(query,answer_of_query);

        free(answer_of_query);
        free(query);

        close(socket_fd);  //Close the connection with the client's
    }
    
    if(error = pthread_mutex_lock(&mutex)) 
    {
        perror("Error with pthread_mutex_lock"); 
        exit(1);
    }
    //Deallocate just one time (one thread) the global array with workers ports
    if(deleted == 0)
    {
        free(worker_ports);
        deleted=1;
    }
    if(error = pthread_mutex_unlock(&mutex)) 
    {
        perror("Error with pthread_mutex_unlock"); 
        exit(1);
    }
}

void Server_Worker_Communicate(int port, char* query, char** answer_of_query)
{
    struct sockaddr_in server_worker_fd;
    struct sockaddr *serverworkerptr = (struct sockaddr *)&(server_worker_fd);
    int Socket_server_worker;

    //Create a TCP socket that server communicate with worker to send him the query
    if((Socket_server_worker = socket(AF_INET , SOCK_STREAM , 0)) < 0)
        perror("Error with creation of socket");
    
    server_worker_fd.sin_family = AF_INET;
    server_worker_fd.sin_addr.s_addr = inet_addr(workerIP);
    (server_worker_fd).sin_port = htons(port);

    //Connect to the a specific worker port to send him the query
    if(connect(Socket_server_worker, serverworkerptr, sizeof(server_worker_fd)) < 0)
        perror("Error with connection");
    
    // printf("Connecting to worker's %s port %d\n", workerIP, port);

    //Write to the worker's socket the query
    Write_Socket(Socket_server_worker,query);
    
    int bytes;

    //Read the worker's answer to the query
    bytes = Read_Socket(Socket_server_worker,answer_of_query);

    //Close the socket that connects server with worker
    close(Socket_server_worker);
}

void Insert_to_Buffer(int socket_fd, int bufferSize)
{
    pthread_mutex_lock(&mutex);
    //Wait until buffer is not full 
    while(buffer.count >= bufferSize) 
    {
        // printf("Buffer is FULL!\n");
        pthread_cond_wait(&condition_non_full, &mutex);
    }
    buffer.end = (buffer.end + 1) % bufferSize;  
    buffer.data[buffer.end] = socket_fd; //Put the new socket descriptor to the buffer
    (buffer.count)++;
    pthread_cond_signal(&condition_non_empty);  //Send signal that is not empty anymore
    pthread_mutex_unlock(&mutex);
}

int Extract_from_Buffer(int bufferSize)
{
    int socket_fd = 0;
    pthread_mutex_lock(&mutex);
    
    //Wait until buffer is not empty
    while(buffer.count <= 0) 
    {
        // printf("Buffer is EMPTY!\n");
        pthread_cond_wait(&condition_non_empty, &mutex);
        //In case main server thread has been received a SIGINT/SIGQUIT signal
        if(flag_server == 1)
        {
            pthread_mutex_unlock(&mutex);
            return -1;
        }
        //In case num of workers has over
        if(num_workers==-10)
        {
            pthread_mutex_unlock(&mutex);
            return -2;
        }
    }

    socket_fd = buffer.data[buffer.start];  //Take a socket descriptor file
    buffer.start = (buffer.start + 1) % bufferSize;
    (buffer.count)--;
    pthread_cond_signal(&condition_non_full);  //Send signal that is not full anymore
    pthread_mutex_unlock(&mutex);
    return socket_fd;
}

void Initialize_Buffer(int bufferSize)
{
    buffer.data = (int *)malloc(bufferSize*sizeof(int));
    buffer.start = 0;
    buffer.end = -1;
    buffer.count = 0;
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

void Print_Statistics_With_Mutexes(char* print_message)
{
    int error;

    if(error = pthread_mutex_lock(&mutex_print)) 
    {
        perror("Error with pthread_mutex_lock"); 
        exit(1);
    }

    printf("%s", print_message);

    if(error = pthread_mutex_unlock(&mutex_print)) 
    {
        perror("Error with pthread_mutex_unlock"); 
        exit(1);
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

void Answer_from_Workers(char* query, char* servIP, char** answer_of_query)
{
    char word[1024];
    int temp=0,num_of_words=0,i;
    int length,p;

    //Take the number of words of the query
    for(p=0; 1==sscanf(query + p, "%s%n", word, &length); p = p + length)
    {   
        num_of_words++;
    }
   
    //Take the fisrt word of the query
    for(p=0; 1==sscanf(query + p, "%s%n", word, &length); p = p + length)
    {
        if(temp == 0)  
        {
            break;
        }
        temp++;
    }

    //Split the cases according to the first word of the query
    if(strcmp(word,"/diseaseFrequency") == 0)
    {
        if((num_of_words == 4) || (num_of_words == 5))
        {
            int count_of_errors=0, count_diseaseFrequency=0;

            //For each worker connect with the worker's
            //port send and take the answer 
            //of client's query.
            for(i=0;i<count_worker_ports;i++)
            {
                char* answer_of_worker; 

                Server_Worker_Communicate(worker_ports[i], query, &answer_of_worker);

                if(strcmp(answer_of_worker, "ERROR") == 0)
                    count_of_errors++;
                else
                    count_diseaseFrequency+=atoi(answer_of_worker);  //Sum all the numbers of outbreaks of all workers
                

                free(answer_of_worker);
            }

            //In case none of worker haw answer to thw query of client
            if(count_of_errors == count_worker_ports)
            {
                (*answer_of_query) = (char*)malloc((strlen("ERROR\n")+1)*sizeof(char));
                strcpy((*answer_of_query),"ERROR\n");
            }
            else  //Else save the num of outbreaks to the answer
            {
                char number[50];

                sprintf(number,"%d",count_diseaseFrequency);

                (*answer_of_query) = (char*)malloc((strlen(number)+1)*sizeof(char));
                strcpy((*answer_of_query),number);
            }
        }
        else  //In case client query is invalid, then do not send it to the workers
        {
            (*answer_of_query) = (char*)malloc((strlen("ERROR\n")+1)*sizeof(char));
            strcpy((*answer_of_query),"ERROR\n");
        }
    }
    else if((strcmp(word,"/topk-AgeRanges") == 0) || (strcmp(word,"/searchPatientRecord") == 0))
    {
        if(((strcmp(word,"/topk-AgeRanges") == 0) && (num_of_words == 6)) || ((strcmp(word,"/searchPatientRecord") == 0) && (num_of_words == 2)))
        {
            int count_of_errors=0;

            //Do the same work as before
            for(i=0;i<count_worker_ports;i++)
            {
                char* answer_of_worker; 

                Server_Worker_Communicate(worker_ports[i], query, &answer_of_worker);

                if(strcmp(answer_of_worker, "ERROR") == 0 || (strcmp(answer_of_worker,"empty") == 0))
                    count_of_errors++;
                else
                {
                    (*answer_of_query) = (char*)malloc((strlen(answer_of_worker)+1)*sizeof(char));
                    strcpy((*answer_of_query),answer_of_worker);

                    free(answer_of_worker);
                    return;
                }
                
                free(answer_of_worker);
            }

            if(count_of_errors == count_worker_ports)
            {
                (*answer_of_query) = (char*)malloc((strlen("ERROR\n")+1)*sizeof(char));
                strcpy((*answer_of_query),"ERROR\n");
            }
        }
        else  //In case client query is invalid, then do not send it to the workers
        {
            (*answer_of_query) = (char*)malloc((strlen("ERROR\n")+1)*sizeof(char));
            strcpy((*answer_of_query),"ERROR\n");
        }
    }
    else if((strcmp(word, "/numPatientAdmissions") == 0) || (strcmp(word, "/numPatientDischarges") == 0))
    {
        //In case client hasn't give country argument
        if(num_of_words == 4)
        {
            int count_of_errors=0;

            for(i=0;i<count_worker_ports;i++)
            {
                char* answer_of_worker; 

                Server_Worker_Communicate(worker_ports[i], query, &answer_of_worker);

                if((strcmp(answer_of_worker, "ERROR") == 0) || (strcmp(answer_of_worker,"empty") == 0))
                {
                    count_of_errors++;
                }
                else
                {
                    if((*answer_of_query)==NULL)
                    {
                        (*answer_of_query) = (char*)malloc((strlen(answer_of_worker)+1)*sizeof(char));
                        strcpy((*answer_of_query),answer_of_worker);
                    }
                    else
                    {
                        (*answer_of_query) = (char*)realloc((*answer_of_query), (strlen((*answer_of_query))+1 ) + (strlen(answer_of_worker)+1));
                        strcat((*answer_of_query),answer_of_worker);
                    }
                }
                
                free(answer_of_worker);
            }

            if(count_of_errors == count_worker_ports)
            {
                (*answer_of_query) = (char*)malloc((strlen("ERROR\n")+1)*sizeof(char));
                strcpy((*answer_of_query),"ERROR\n");
            }
        }
        else if(num_of_words == 5)  //In case client give country argument
        {
            int count_of_errors=0;
            int num_of_outbreaks=0;
            char country[30];

            for(i=0;i<count_worker_ports;i++)
            {
                char* answer_of_worker; 

                Server_Worker_Communicate(worker_ports[i], query, &answer_of_worker);

                if(strcmp(answer_of_worker, "ERROR") == 0)
                    count_of_errors++;
                else
                {
                    char word[30];
                    int length,p;
                    int temp=0;

                    //Take the number of outbreaks of each answer
                    for(p=0; 1==sscanf(answer_of_worker + p, "%s%n", word, &length); p = p + length)
                    {
                        if(temp == 1)  
                        {
                            break;
                        }
                        temp++;
                    }

                    temp=0;

                    //Take the country of the answre
                    for(p=0; 1==sscanf(answer_of_worker + p, "%s%n", country, &length); p = p + length)
                        break;

                    num_of_outbreaks += atoi(word); //Sum the outbreaks            
                }
                
                free(answer_of_worker);
            }

            if(count_of_errors == count_worker_ports)
            {
                (*answer_of_query) = (char*)malloc((strlen("ERROR\n")+1)*sizeof(char));
                strcpy((*answer_of_query),"ERROR\n");
            } 
            else
            {
                //Malloc to the answer of the query the country of the query with its outbreaks
                if((*answer_of_query)==NULL)
                {
                    (*answer_of_query) = (char*)malloc((strlen(country)+1)*sizeof(char));
                    strcpy((*answer_of_query),country);

                    (*answer_of_query) = (char*)realloc((*answer_of_query), (strlen((*answer_of_query))+1)+2);
                    strcat((*answer_of_query)," ");

                    char outbreaks[30];

                    sprintf(outbreaks,"%d",num_of_outbreaks); 

                    (*answer_of_query) = (char*)realloc((*answer_of_query), (strlen((*answer_of_query))+1 ) + (strlen(outbreaks)+1));
                    strcat((*answer_of_query),outbreaks);  

                    (*answer_of_query) = (char*)realloc((*answer_of_query), (strlen((*answer_of_query))+1)+3);
                    strcat((*answer_of_query),"\n");
                }
            }
        }
        else  //In case client query is invalid, then do not send it to the workers
        {
            (*answer_of_query) = (char*)malloc((strlen("ERROR\n")+1)*sizeof(char));
            strcpy((*answer_of_query),"ERROR\n");
        } 
    } //In case client query is invalid, then do not send it to the workers
    else
    {
        (*answer_of_query) = (char*)malloc((strlen("ERROR\n")+1)*sizeof(char));
        strcpy((*answer_of_query),"ERROR\n");
    }
}

void Get_ServIP(char** servIP)
{
    char host[256];
    char *IP;
    struct hostent *host_entry;
    int hostname;
    
    //Get the host name 
    hostname = gethostname(host, sizeof(host));

    if(hostname == -1) 
    {
      perror("gethostname");
      exit(1);
    }

    //Get host information
    host_entry = gethostbyname(host);
    
    if(host_entry == NULL)
    {
      perror("gethostbyname");
      exit(1);
    }

    //Convert the address into string (char*)
    IP = inet_ntoa(*((struct in_addr*) host_entry->h_addr_list[0]));
    // (*servIP) = IP;
    (*servIP) = (char*)malloc((strlen(IP)+1)*sizeof(char));
    strcpy((*servIP),IP);
}