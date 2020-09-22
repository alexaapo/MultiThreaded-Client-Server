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
#include "worker_info.h"
#include "read_write.h"
#include "handler.h"

#define PERMS 0666

volatile sig_atomic_t flag=0;
volatile sig_atomic_t worker_pid=-10;

int main(int argc, char** argv)
{
    //Check for invalid input.
    if( argc < 11 || argc > 11 )
    {
        printf("Invalid Input. Please give 7 arguments.\n");
        return(1);
    }
    
    int  numWorkers, bufferSize, i;
    char* input_dir, *serverIP, *serverPort;

    //In case we give the arguments with random sequence.
    for(i=1;i<=9;i+=2)
    {
        if(strcmp(argv[i],"-i") == 0)
        {
            input_dir = (char*)malloc((strlen(argv[i+1])+1)*sizeof(char));
            strcpy(input_dir,argv[i+1]);
        }
        else if(strcmp(argv[i],"-s") == 0)
        {
            serverIP = (char*)malloc((strlen(argv[i+1])+1)*sizeof(char));
            strcpy(serverIP, argv[i+1]);
        }
        else if(strcmp(argv[i],"-p") == 0)
        {
            serverPort = (char*)malloc((strlen(argv[i+1])+1)*sizeof(char));
            strcpy(serverPort, argv[i+1]);
        }
        else if(strcmp(argv[i],"-w") == 0)
            numWorkers = atoi(argv[i+1]);
        else if(strcmp(argv[i],"-b") == 0)
            bufferSize = atoi(argv[i+1]);
        else
        {
            printf("Please give an input with this form: ./diseaseAggregator –w numWorkers -b bufferSize -i input_dir\n");
            exit(1);
        }
    }

    //Check for validation of bufferSize and num of Workers.
    if((bufferSize==0) || (numWorkers==0))
    {
        printf("Please give at least 1 in buffersize/numWorkers\n");
        free(input_dir);
        exit(1);
    }
    
    //Open the directory to take the county names and save them in an array.
    struct dirent *ent;
    DIR *dir = opendir(input_dir);
    if(!dir)
    {
        printf("ERROR: Please provide a valid directory path.\n");
        free(input_dir);
        exit(1);
    }

    //Take the number of Countries.
    int num_of_countries=0, k=0;
    while ((ent = readdir (dir)) != NULL)
    {
        if(strcmp(ent->d_name,".") != 0 && strcmp(ent->d_name,"..") != 0)
            num_of_countries++;
    }
    closedir (dir);

    //Save the country names in an array.
    dir = opendir(input_dir);
    char** countries_array = (char**)malloc(num_of_countries*sizeof(char*));
    // printf("%d\n",num_of_countries);
    while ((ent = readdir (dir)) != NULL)
    {
        if((strcmp(ent->d_name,".") != 0) && (strcmp(ent->d_name,"..") != 0))
        {
            countries_array[k] = (char*)malloc((strlen(ent->d_name)+1)*sizeof(char));
            strcpy(countries_array[k],ent->d_name);
            k++;
        }
    }
    closedir (dir);

    // for(i=0;i<num_of_countries;i++)  printf("%s\n",countries_array[i]);

    //Now I "split" the directories to Workers.
    int times=0, directories_per_child, left_over_workers=0,left_over_directories=0;
    k=0;

    //In case countries are more than Workers,
    //then there are leftover directories.
    if(num_of_countries > numWorkers)
    {
        directories_per_child = num_of_countries/numWorkers;
        left_over_directories = num_of_countries%numWorkers;
        times=numWorkers;
    }
    //In case countries are less than Workers,
    //then each Worker will take 1 directory.
    else 
    {
        directories_per_child = 1;
        times=num_of_countries; //This is for inactive Workers.
    }

    char pipe_P2C[100];
    pid_t pid;

    worker_info worker_info_array[times];
    // printf("times %d numworkers %d\n",times,numWorkers);

    for( i=0 ; i<numWorkers ; i++)
    {
        //In case of active Workers.
        if(i < times)
        {
            //Create the FIFOS.
            sprintf(pipe_P2C,"Pipe_P2C_%d", i+1);
            // sprintf(pipe_C2P,"Pipe_C2P_%d", i+1);

            if (mkfifo(pipe_P2C, PERMS) < 0) 
                perror("Fail to create a named Pipe.\n");
            // if (mkfifo(pipe_C2P, PERMS) < 0) 
            // {
            //     // unlink(pipe_P2C);
            //     perror("Fail to create a named Pipe.\n");
            // }
            //Save the names of pipes
            strcpy(worker_info_array[i].pipe_P2C,pipe_P2C);
            // strcpy(worker_info_array[i].pipe_C2P,pipe_C2P);
            worker_info_array[i].num_of_directories=0;
            
            //In case we have left over directories, there are some Workers that will take more directories than others.
            if(left_over_directories > 0 )
            {
                int j;
                worker_info_array[i].worker_directories = (char**)malloc((directories_per_child+1)*sizeof(char*));
                worker_info_array[i].num_of_directories = directories_per_child+1;
                for(j=0 ; j<directories_per_child+1; j++)
                {
                    worker_info_array[i].worker_directories[j] = (char*)malloc((strlen(countries_array[k])+1)*sizeof(char));
                    strcpy(worker_info_array[i].worker_directories[j],countries_array[k]);
                    k++;
                }
                left_over_directories--;
            }
            else if(left_over_directories==0)
            {
                int j;
                worker_info_array[i].worker_directories = (char**)malloc((directories_per_child)*sizeof(char*));
                worker_info_array[i].num_of_directories = directories_per_child;
                for(j=0 ; j<(directories_per_child); j++)
                {
                    worker_info_array[i].worker_directories[j] = (char*)malloc((strlen(countries_array[k])+1)*sizeof(char));
                    strcpy(worker_info_array[i].worker_directories[j],countries_array[k]);
                    k++;
                }
            }
        }
        
        //Create Workers
        if((pid = fork()) == -1)
        {
            printf("Faild to fork\n");
            exit(1);
        }
        else if(pid == 0)
        {
            if(i<times)  //Exec the program for each active Worker
            {
                char buffersize[10];
                sprintf(buffersize,"%d",bufferSize);
                // printf("%s\n",buffersize);
                char* args[] = {"./worker", worker_info_array[i].pipe_P2C, buffersize, NULL};
                execv("./worker", args);
            }
            else  //In case of inactive workers
            {
                char inactive[5];
                sprintf(inactive,"%d",-1);
                char* args1[] = {"./worker", inactive, NULL};
                execv("./worker", args1); 
            }
        }
        
        if(i<times)  worker_info_array[i].pid = pid;  //Save the pid of each Worker in the struct
               
    }
    
    // printf("Parent id: %d\n", getpid());
    
    int j;
    char num_workers[50];
    sprintf(num_workers,"%d",times);

    //Open the pipe P2C for sending to the Worker the informations about his directories
    for(i=0;i<times;i++)
    {

        //Save the information that want to send in a Worker in an array.
        //Communication protocol:
        //1st line: name of input dir.
        //2nd line: number of directories.
        //3rd line: number of active Workers.
        //4th line: number of serverPort.
        //5th line: number of serverIP.
        //6rd line: directories of Worker.
        char msg[25*num_of_countries];
        char num_of_dir[20];
        sprintf(num_of_dir,"%d",worker_info_array[i].num_of_directories);

        strcpy(msg,"");

        strncat(msg,input_dir,strlen(input_dir));
        strncat(msg,"\n",1);

        strncat(msg,num_of_dir,strlen(num_of_dir));
        strncat(msg,"\n",1);

        // strncat(msg,worker_info_array[i].pipe_P2C,strlen(worker_info_array[i].pipe_P2C));
        // strncat(msg,"\n",1);
        
        strncat(msg,num_workers,strlen(num_workers));
        strncat(msg,"\n",1);

        strncat(msg,serverPort,strlen(serverPort));
        strncat(msg,"\n",1);

        strncat(msg,serverIP,strlen(serverIP));
        strncat(msg,"\n",1);

        //Save the name of directories of each Worker.
        for(j=0;j<worker_info_array[i].num_of_directories;j++)
        {
            strncat(msg,worker_info_array[i].worker_directories[j],strlen(worker_info_array[i].worker_directories[j]));
            if((j<worker_info_array[i].num_of_directories-1))  strncat(msg," ",1);
            else  strncat(msg,"\0",1); 
        }
        
        //Start writing in the pipe P2C with size bufferSize
        Write_Pipe(msg,worker_info_array[i].pipe_P2C,bufferSize);
    }


    //For signal SIGINT
    struct sigaction act;
 
	memset(&act, 0, sizeof(act));
	act.sa_sigaction = Handler;
	act.sa_flags = SA_SIGINFO;
 
	if (sigaction(SIGINT, &act, NULL) < 0) {
		perror ("Sigaction\n");
		return 1;
	}

    //For signal SIGQUIT
    struct sigaction act1;
 
	memset(&act1, 0, sizeof(act1));
	act1.sa_sigaction = Handler;
	act1.sa_flags = SA_SIGINFO;
 
	if (sigaction(SIGQUIT, &act1, NULL) < 0) {
		perror ("Sigaction\n");
		return 1;
	}

    //For signal SIGCHLD (useful for parent to know when a Worker has ended)
    struct sigaction act2;
 
	memset(&act2, 0, sizeof(act2));
	act2.sa_sigaction = Handler_New_Worker;
	act2.sa_flags = SA_SIGINFO | SA_RESTART;
 
	if (sigaction(SIGCHLD, &act2, NULL) < 0) {
		perror ("Sigaction\n");
		return 1;
	}

    // printf("Parent PID: %d\n",getpid());

    //Begin the application of diseaseMonitor
    while(1)
    {

        //In case the Parent accept a SIGINT/SIGQUIT signal
        if(flag == 1)
        {
            int z = 0;
            for (z; z < times; z++) wait(NULL);

            //Restart the flag for SIGINT/SIGQUIT
            flag=0;          

            break;
        }
    }
    
    //Deallocate memory.
    for(i=0; i<times ; i++)
    {
        if(remove(worker_info_array[i].pipe_P2C) != 0)
            printf("Fail to delete the named Pipe.\n");
    }

    for(i=0; i<times; i++)
    {
        for(j=0;j<(worker_info_array[i].num_of_directories);j++)
            free(worker_info_array[i].worker_directories[j]);
        free(worker_info_array[i].worker_directories);
    }

    for(i=0; i<num_of_countries; ++i)
        free(countries_array[i]);
    free(countries_array);

    free(input_dir);
    free(serverIP);
    free(serverPort);

    return 0;
}
