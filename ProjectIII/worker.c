#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <signal.h>
#include <arpa/inet.h>

#include "read_write.h"
#include "list.h"
#include "handler_worker.h"

volatile sig_atomic_t flag1=0;
volatile sig_atomic_t flag2=0;

int main(int argc, char** argv)
{
    // printf("Child id: %d with P2C: %s\n", getpid(), argv[1]);
    
    //In case of inactive Workers
    if(atoi(argv[1])==-1)  
    {
        // printf("This is a inactive Worker with id: %d\n",getpid());
        return 0;
    }

    //Take the name of th pipe P2C from the Parent and the buffersize.
    char pipe_P2C[20];
    strcpy(pipe_P2C,argv[1]);  
    int bufferSize = atoi(argv[2]), P2C_fd;  
    
    char buf[bufferSize];
    strcpy(buf,"");

    //Open the pipe P2C for reading.
    if((P2C_fd = open(pipe_P2C, O_RDONLY))  < 0)  
        perror("Worker: can't open read fifo");
    
    
    //Read the message from parent in size of bufferSize.
    //In case the messsage is bigger bytes than bufferSize, 
    //then I "cut" the message into bufferSize bytes and 
    //put them all together with the use of realloc().
    int k=0,bytes;
    k=read(P2C_fd, buf, bufferSize);
    bytes=k;
    
    char* final_msg;

    final_msg = (char *)malloc((k+1)*sizeof(char));
    strcpy(final_msg,"");  //Initialize the array with the final message.

   	while(k!=0)
   	{
		strncat(final_msg,buf,k);
		k=read(P2C_fd, buf, bufferSize);
        if(k!=0)
        { 
            bytes+=k;
            final_msg = (char *)realloc(final_msg, (bytes+k+1)*sizeof(char));
        }
	}
    strncat(final_msg,"\0",1);  //Put the null character in the end of message.
    final_msg[bytes] = '\0';

    close(P2C_fd);

    int i=0,j=0, num_of_directories, line=0;
    char num_of_workers[30], temp[bytes], input_dir[20], serverPort[30], serverIP[30];
    char** directories;
    strcpy(temp,"");

    //Now I have to translate the message that the parent send,
    //according to the communication protocol, so to take the 
    //appropriate information the Worker needs.

    while(final_msg[i] != '\0')
    {
        while((final_msg[i] != '\n') && (final_msg[i] != '\0'))
        {            
            //1st line: name of input dir.
            //2nd line: number of directories.
            //3rd line: number of active workers.
            //4th line: number of serverPort.
            //5th line: number of serverIP.
            //6rd line: directories of Worker.
            if(line != 5)  strncat(temp,&final_msg[i],1);
            else
            {
                while((final_msg[i] != ' ') && (final_msg[i] != '\0'))
                {
                    strncat(temp,&final_msg[i],1);
                    i++;
                }
                directories[j] = (char*)malloc((strlen(final_msg)+1)*sizeof(char));
                strcpy(directories[j],temp);
                strcpy(temp,"");
                j++;
            }
            
            if(final_msg[i] != '\0')  i++;
        }
        if(line == 0)
        {
            strcpy(input_dir,temp);
        }
        if(line == 1)  
        {
            num_of_directories = atoi(temp);
            directories = (char**)malloc((num_of_directories)*sizeof(char*));
        }
        if(line == 2)
        {  
            strcpy(num_of_workers,temp);
        }
        if(line == 3)
        {  
            strcpy(serverPort,temp);
        }
        if(line == 4)
        {  
            strcpy(serverIP,temp);
        }
        line++;
        strcpy(temp,"");
        if(final_msg[i] != '\0')  i++;
    }

    free(final_msg);
    
    // char s[100];
    // printf("%s\n", getcwd(s, 100));

    //After we have all the information the Worker needs,
    //we go to his directories and read the files.
    struct dirent *ent;
    DIR *dir = opendir(input_dir);
    if(!dir)
    {
        printf("ERROR: Please provide a valid directory path.\n");
        exit(1);
    }
    
    chdir(input_dir);  //Change the directory cd input_dir
    
    //Find the latest date for helping us later.
    char date[11];
    char max[11];
    int num_of_date_files=0,num_of_rec_per_file=0,temp1=0;
    strcpy(max,"");

    char** max_dates=(char**)malloc(num_of_directories*sizeof(char*));
    for(i=0;i<num_of_directories;i++)
        max_dates[i] = (char*)malloc(11*sizeof(char));

    listptr head = NULL;
    int num_of_words=0;
    
    //This is for finding the number of records per file, the number of date files in each subdirectory and the number of Worker's diseases.
    for(i=0;i<num_of_directories;i++)
    {
        DIR *subdir = opendir(directories[i]);
        if(!subdir)
        {
            printf("ERROR: Please provide a valid directory path.\n");
            exit(1);
        }
        if(chdir(directories[i]) != 0)
        {
            perror("Can't change directory.\n");
            return 1;
        }
        while((ent = readdir (subdir)) != NULL)
        {
            if(strcmp(ent->d_name,".") != 0 && strcmp(ent->d_name,"..") != 0)
            {    
                if(i==0)  num_of_date_files++;
                
                FILE* fp;
                char buf[1024];
                if ((fp = fopen(ent->d_name, "r")) == NULL)
                { /* Open source file. */
                    perror("Can't open the file.");
                    return 1;
                }
                while (fgets(buf, sizeof(buf), fp) != NULL)
                {
                    char word[1024];
                    int length,p;
                    if(i==0 && temp1==0)  num_of_rec_per_file++;
                    
                    int temp=0;

                    for(p=0; 1==sscanf(buf + p, "%s%n", word, &length); p = p + length)
                    {   
                        num_of_words++;
                    }

                    if(num_of_words == 6) //Check for the validity of record
                    {
                        for(p=0; 1==sscanf(buf + p, "%s%n", word, &length); p = p + length)
                        {
                            if(temp == 4)  
                            {
                                Insert_List(&head,word);  //Put the name of disease in a list
                                break;
                            }
                            temp++;
                        }
                    }
                    
                    num_of_words=0;
                }
                fclose(fp);
                temp1=1;
            }
        }
        closedir(subdir);

        if(chdir("..") != 0)
        {
            perror("Can't change directory.\n");
            return 1;
        }
    }

    // Print_List(head);
    int num_of_diseases = List_Size(head);
    // printf("num of date files: %d\n",num_of_date_files);
    // printf("num of records per file: %d\n",num_of_rec_per_file);
    // printf("num of diseases: %d\n",num_of_diseases);
    Delete_List(&head);

    //I create an array of dates of each directory, so I will know the latest date for each directory (This will help with SIGUSR1 later)
    char *** dates_array = (char***)malloc(num_of_directories*sizeof(char**));
    for(i=0;i<num_of_directories;i++)
    {
        dates_array[i] = (char**)malloc(num_of_date_files*sizeof(char*));
        for(j=0;j<num_of_date_files;j++)
            dates_array[i][j] = (char*)malloc(11*sizeof(char));
    }

    for(i=0;i<num_of_directories;i++)
    {
        DIR *subdir = opendir(directories[i]);
        if(!subdir)
        {
            printf("ERROR: Please provide a valid directory path.\n");
            exit(1);
        }

        if(chdir(directories[i]) != 0)
        {
            perror("Can't change directory.\n");
            return 1;
        }
        int dates=0;
        while((ent = readdir (subdir)) != NULL)
        {
            //Take the names of dates and copy them to the array of dates.
            if(strcmp(ent->d_name,".") != 0 && strcmp(ent->d_name,"..") != 0)
            {    
                strcpy(date,"");
                memcpy(date,ent->d_name,10);
                date[10] = '\0';

                strcpy(dates_array[i][dates],date);
                dates++;
            }
        }

        //Sort the array of dates of each directory
        qsort(dates_array[i], num_of_date_files, sizeof(char*), &Compare_Dates);
        //Save to the array max_dates the last date of sorted array. So now I know the latest date of each directory.
        strcpy(max_dates[i],dates_array[i][num_of_date_files-1]);

        for(j=0;j<num_of_date_files;j++)
        {
            dates_array[i][j] = (char *) realloc(dates_array[i][j], 16);
            strcat(dates_array[i][j], ".txt");
        }
        
        closedir(subdir);
        if(chdir("..") != 0)
        {
            perror("Can't change directory.\n");
            return 1;
        }
    }

    
    // for(i=0;i<num_of_directories;i++)
    // {
    //     printf("%s\n",directories[i]);
    //     for(j=0;j<num_of_date_files;j++)
    //     {
    //         printf("%s\n",dates_array[i][j]);
    //     }
    //     printf("Max Date for %s is %s\n",directories[i],max_dates[i]);
    // }

    //Now that the Worker has all the information that he needs, he stores into the data structures (from the 1st project) the records.
    int x,l;
    num_of_words=0;
    bucket_ptr disease_array[num_of_diseases];  //Disease Hash Table
    bucket_ptr country_array[num_of_directories];  //Country Hash Table
    rec_ptr head1 = NULL;

    //Initialize the arrays of buckets.
    for(i=0; i < num_of_diseases; i++)
        disease_array[i] = NULL;
    
    for(i=0; i < num_of_directories; i++)
        country_array[i] = NULL;

    //I store the records of each subdirectory and each date file into an array. 
    //This is gona help me to have complete records just like the 1st project with
    //id name surname disease country age entry exit.
    for(i=0;i<num_of_directories;i++)
    {
        int z=0;
        char *** records = (char***)malloc((num_of_rec_per_file*num_of_date_files)*sizeof(char**));
        for(x=0;x<(num_of_rec_per_file*num_of_date_files);x++)
        {
            records[x] = (char**)malloc(8*sizeof(char*));
        }
        DIR *subdir = opendir(directories[i]);
        if(!subdir)
        {
            printf("ERROR: Please provide a valid directory path.\n");
            exit(1);
        }
        if(chdir(directories[i]) != 0)
        {
            perror("Can't change directory.\n");
            return 1;
        }
        //For each date file
        for(j=0;j<num_of_date_files;j++)
        {
            FILE* fp;
            char buf[1024];
            if ((fp = fopen(dates_array[i][j], "r")) == NULL)
            { 
                perror("Can't open the file.");
                return 1;
            }
            //For each record
            while (fgets(buf, sizeof(buf), fp) != NULL)
            {
                buf[strlen(buf) - 1] = '\0'; //eat the newline fgets() stores

                char word[1024];
                int length,p,temp=0;
                
                //Take the number of words of each record, because if a record has less words than 6, 
                //its surely invalid, so do not save it into the array.
                for(p=0; 1==sscanf(buf + p, "%s%n", word, &length); p = p + length)
                {   
                    num_of_words++;
                }

                if(num_of_words == 6)
                {
                    for(p=0; 1==sscanf(buf + p, "%s%n", word, &length); p = p + length)
                    {
                        if(temp == 1)  //Word 1 is the "Enter" or "Exit"
                        {
                            char dt[11]="";
                            memcpy(dt,dates_array[i][j],10);

                            //Save the appropriate date to Enter or Exit index of the array.
                            if(strcmp(word,"ENTER") == 0)
                            {
                                records[z][6] = (char*)malloc((strlen(dt)+1)*sizeof(char));
                                strcpy(records[z][6],dt);

                                records[z][7] = (char*)malloc(3*sizeof(char));
                                strcpy(records[z][7],"--");
                            }
                            else if(strcmp(word,"EXIT") == 0)
                            {
                                records[z][7] = (char*)malloc((strlen(dt)+1)*sizeof(char));
                                strcpy(records[z][7],dt);

                                records[z][6] = (char*)malloc(3*sizeof(char));
                                strcpy(records[z][6],"--");
                            }
                            else //If the 2nd word is not "Enter" neither "exit" then this record is invalid.
                            {
                                strcpy(records[z][0],"-");

                                records[z][6] = (char*)malloc(2*sizeof(char));
                                strcpy(records[z][6],"-");

                                records[z][7] = (char*)malloc(2*sizeof(char));
                                strcpy(records[z][7],"-");

                                printf("ERROR\n");
                            }
                            temp++;
                            continue;
                        }
                        else if(temp == 5)  //Word 5 is the age but first I have to store the country
                        {
                            records[z][temp-1] = (char*)malloc((strlen(directories[i])+1)*sizeof(char));
                            strcpy(records[z][temp-1],directories[i]);

                            records[z][temp] = (char*)malloc((strlen(word)+1)*sizeof(char));
                            strcpy(records[z][temp],word);
                        }
                        else if(temp>1) //For name, surname, disease
                        {
                            records[z][temp-1] = (char*)malloc((strlen(word)+1)*sizeof(char));
                            strcpy(records[z][temp-1],word);
                        }
                        else  //For record id
                        {
                            records[z][temp] = (char*)malloc((strlen(word)+1)*sizeof(char));
                            strcpy(records[z][temp],word);
                        }
                        
                        temp++;
                    }

                }
                else  printf("ERROR\n");
                num_of_words=0;
                z++;
            }
            fclose(fp);
            
        }
        
        closedir(subdir);

        //Go to input_dir
        if(chdir("..") != 0)
        {
            perror("Can't change directory.\n");
            return 1;
        }

        // for(x=0;x<(num_of_rec_per_file*num_of_date_files);x++)
        // {
        //     for(l=0;l<8;l++)
        //     {
        //         printf("%s ",records[x][l]);
        //     }
        //     printf("\n");
        // }


        //Now Worker is ready to edit half records who holds them in array of records so as to create a complete record
        for(x=0;x<(num_of_date_files*num_of_rec_per_file);x++)
        {
            //In case of Enter record
            if(strcmp(records[x][6],"--") != 0)
            {
                //Find into the array the EXIT record with same record ID.(if exists)
                for(l=0;l<(num_of_date_files*num_of_rec_per_file);l++)
                {
                    //If ENTER record and EXIT record have the same record ID update exit_date so as to have a complete record.
                    if(strcmp(records[x][0],records[l][0]) == 0 && strcmp(records[l][7],"--") != 0 && strcmp(records[l][0],"-") !=0)
                    {
                        free(records[x][7]);
                        records[x][7] = (char*)malloc((strlen(records[l][7])+1)*sizeof(char));
                        strcpy(records[x][7],records[l][7]);
                        
                        strcpy(records[l][0],"-");
                        break;
                    }
                }
            }
        }

        // printf("\n");
        // for(x=0;x<(num_of_rec_per_file*num_of_date_files);x++)
        // {
        //     // Insert_List(&head,records[x][3]);
        //     for(l=0;l<8;l++)
        //     {
        //         printf("%s ",records[x][l]);
        //     }
        //     printf("\n");
        // }

        //Now that I have the complete records I start to insert them into the structurs.
        int bucket_size=20;

        for(x=0;x<(num_of_rec_per_file*num_of_date_files);x++)
        {
            if(strcmp(records[x][0],"-")!=0)
            {
                //Check if the Record is valid
                int error = Check_Validity_of_Record(&head1,records[x][0],records[x][1],records[x][2],records[x][3],records[x][4],records[x][5],records[x][6],records[x][7]);
                if(error == 1)
                {
                    printf("Record ID is already exists!\n");
                    exit(1);
                }
                if(error!=0)
                    Print_Error(error); 
                else //If the records are valid put them in the records list.
                {
                    rec_ptr record_node;
                    int hash, hash1;   

                    record_node = Insert_Record(&head1,records[x][0],records[x][1],records[x][2],records[x][3],records[x][4],records[x][5],records[x][6],records[x][7]);

                    hash = Hash_Function(records[x][3], num_of_diseases);
                    hash1 = Hash_Function(records[x][4], num_of_directories);

                    //For the Hash table of Diseases
                    //Case 1: There is not any created bucket yet.
                    if(disease_array[hash] == NULL)
                    {
                        bucket_ptr new_bucket;

                        new_bucket = Create_Bucket(bucket_size);
                        
                        if(new_bucket != NULL) disease_array[hash] = new_bucket;
                        else printf("Problem with memory!\n");

                        Insert_Bucket_Entry(new_bucket,record_node,records[x][3],records[x][6],bucket_size);
                    }
                    else //Case 2: If there at least one bucket.
                    {
                        bucket_ptr current = disease_array[hash];
                        //Insert the disease to the apropriate bucket.
                        Insert_Bucket_Entry(current,record_node,records[x][3],records[x][6],bucket_size);
                    }

                    //For the Hash table of Countries
                    //Case 1: There is not any created bucket yet.
                    if(country_array[hash1] == NULL)
                    {
                        bucket_ptr new_bucket;

                        new_bucket = Create_Bucket(bucket_size);
                        
                        if(new_bucket != NULL) country_array[hash1] = new_bucket;
                        else printf("Problem with memory!\n");

                        Insert_Bucket_Entry(new_bucket,record_node,records[x][4],records[x][6],bucket_size);
                    }
                    else //Case 2: If there at least one bucket.
                    {
                        bucket_ptr current = country_array[hash1];
                        //Insert the disease to the apropriate bucket.
                        Insert_Bucket_Entry(current,record_node,records[x][4],records[x][6],bucket_size);
                    }                       
                }
            }
        }

        //Deallocate the memory of records array
        for(x=0;x<(num_of_rec_per_file*num_of_date_files);x++)
        {
            for(l=0;l<8;l++)
                free(records[x][l]);
            free(records[x]);
        }
        free(records);
    }
    // Print_Record(head1);
     //printf("Number of records that have inserted: %d\n",Size_of_List(head1));

    //Find the Summary Statistics form the country hash table and store them into an array, so to send them to the parent to print them.
    char *statistics = NULL;
    for(i=0;i<num_of_directories;i++)
    {
        bucket_ptr current = country_array[i];
        if(current!= NULL)
        {
            // printf("Bucket %d\n",i);
            while(current != NULL)  
            {
                for(j=0;j<current->entry_counter;j++)
                {
                    // Statistics_AVL(country_array[i]->name[j]->avl_tree_root,country_array[i]->name[j]->name,&statistics);
                    Statistics_AVL(current->name[j]->avl_tree_root,current->name[j]->name,&statistics);
                }
                current = current->next;
            }
        }
    }

    closedir (dir);

    if(chdir("..") != 0)
    {
        perror("Can't change directory.\n");
        return 1;
    }

    struct sockaddr_in worker_server_fd, server_worker_fd;
    socklen_t server_worker_len = sizeof(server_worker_fd);
    socklen_t worker_server_len = sizeof(worker_server_fd);
    int worker_server_socket, server_worker_socket;

    struct sockaddr *workerserverptr = (struct sockaddr *)&worker_server_fd;
    struct sockaddr *serverworkerptr = (struct sockaddr *)&server_worker_fd;
    
    char* workerIP;
    //Get the IP address of worker
    Get_ServIP(&workerIP);

    //Create a TCP socket to listen server's thread connections.
    if((server_worker_socket = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        perror("Error with creation of the socket");
    
    server_worker_fd.sin_family = AF_INET ;
    server_worker_fd.sin_addr.s_addr = inet_addr(workerIP);
    server_worker_fd.sin_port = htons(0);  //Create new port for each worker 
    int one = 1;


    //Make port reusable.
    if(setsockopt(server_worker_socket, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(int)) < 0)  
    { 
        perror("Error with setsockopt(SO_REUSEADDR)");
        exit(1);
    }

    if(bind(server_worker_socket, serverworkerptr, sizeof(server_worker_fd)) < 0)
    {
        perror("Error with bind");
        exit(1);
    }
    
    //Obtain worker's port
    if(getsockname(server_worker_socket,(struct sockaddr*)&server_worker_fd,&server_worker_len) < 0)
    {
        perror("Error with getsockname");
        exit(1);
    }
    int workerPort = ntohs(server_worker_fd.sin_port);

    if(listen(server_worker_socket, 1000) < 0)
    {
        perror("Error with listen");
        exit(1);
    }

    printf("Listening for conections to port %d to IP %s\n", workerPort, workerIP);

    //Create a TCP temporary socket that writes to server threads the appropriate informations.
    if((worker_server_socket = socket(AF_INET , SOCK_STREAM , 0)) < 0)
        perror("Error with creation of socket");
    
    worker_server_fd.sin_family = AF_INET ;
    worker_server_fd.sin_addr.s_addr = inet_addr(serverIP);
    worker_server_fd.sin_port = htons(atoi(serverPort));

    if(connect(worker_server_socket, workerserverptr, sizeof(worker_server_fd)) < 0)
        perror("Error with connection");
    
    // printf("Connecting to %s port %d.\n", serverIP, atoi(serverPort));

    char worker_port[30];
    sprintf(worker_port,"%d",workerPort);

    char* message;

    //1st line: number of workers.
    //2nd line: worker port number.
    //3rd line: statistics.

    message = (char*)malloc((strlen(num_of_workers)+1)*sizeof(char));
    strcpy(message,num_of_workers);

    message = (char*)realloc(message, (strlen(message)+1)+3);
    strcat(message,"\n");

    message = (char*)realloc(message, (strlen(message)+1) + (strlen(worker_port)+1));
    strcat(message,worker_port);

    message = (char*)realloc(message, (strlen(message)+1)+3);
    strcat(message,"\n");

    message = (char*)realloc(message, (strlen(message)+1) + (strlen(statistics)+1));
    strcat(message,statistics);

    message = (char*)realloc(message, (strlen(message)+1)+3);
    strcat(message,"\0");

    size_t size_array;

    size_array = (strlen((message))+1)/sizeof(message[0]);
        
    //Write to server threads the previous informations of each worker
    for(j=0;j<(size_array);j++)
    {
        if(write(worker_server_socket, &message[j], 1) < 0)
        {
            perror("Error with write");
            exit(1);
        }
    }

    free(message);

    close(worker_server_socket);

    //For SIGINT signal
    struct sigaction act;
 
	memset(&act, 0, sizeof(act));
	act.sa_sigaction = Handler1;
	act.sa_flags = SA_SIGINFO;
 
	if (sigaction(SIGINT, &act, NULL) < 0) {
		perror ("Sigaction\n");
		return 1;
	}

    //For SIGQUIT signal
    struct sigaction act1;
 
	memset(&act1, 0, sizeof(act1));
	act1.sa_sigaction = Handler1;
	act1.sa_flags = SA_SIGINFO;
 
	if (sigaction(SIGQUIT, &act1, NULL) < 0) {
		perror ("Sigaction\n");
		return 1;
	}

    //For SIGUSR1 signal
    struct sigaction act2;
 
	memset(&act2, 0, sizeof(act2));
	act2.sa_sigaction = Handler_Usr1;
	act2.sa_flags = SA_SIGINFO | SA_RESTART;
 
	if (sigaction(SIGUSR1, &act2, NULL) < 0) {
		perror ("Sigaction\n");
		return 1;
	}

    //Begin the application of diseaseMonitor
    while(1)
    {
        char* line;
        int new_server_worker_socket;

        //Accept server threads connection
        if((new_server_worker_socket = accept(server_worker_socket, serverworkerptr, &server_worker_len)) < 0)
        {
            //In case worker has been received SIGINT/SIGQUIT signal terminates.
            if(flag1 == 1)
            {      
                flag1=0;
                break;
            }
            perror("Error with accept");
            exit(1);
        }
        // printf("Connection accepted.\n");

        //Read the client's query
        Read_Socket(new_server_worker_socket, &line);
        
        //Split the query into words
        char user_input[10][100];
        strcpy(user_input[4],"0");
        int w=0,j=0;
        for(i=0;i<=(strlen(line));i++)
        {
            // if space or NULL found, assign NULL into user_input[w]
            if(line[i]==' '|| line[i]=='\0')
            {
                user_input[w][j]='\0';
                w++;  //for next word
                j=0;    //for next word, init index to 0
            }
            else
            {
                user_input[w][j]=line[i];
                j++;
            }
        }
        
        //Split the cases according to the first word of each query
        if(strcmp(user_input[0],"/diseaseFrequency") == 0)
        {   
            char* entry_date, *exit_date, *virusName, *country = NULL;
            int j, error=0,error1=0,error2=0,error3=0,error4=0;

            //First of all I have to check the Validity of User Input

            virusName = (char*)malloc((strlen(user_input[1])+1)*sizeof(char));
            strcpy(virusName,user_input[1]);

            entry_date = (char*)malloc((strlen(user_input[2])+1)*sizeof(char));
            strcpy(entry_date,user_input[2]);

            exit_date = (char*)malloc((strlen(user_input[3])+1)*sizeof(char));
            strcpy(exit_date,user_input[3]);  

            error = Invalid_Date(entry_date);
            
            if(strcmp(user_input[3], "--") != 0)
            {
                error1 = Invalid_Date(exit_date);

                if(date_to_seconds(entry_date)>date_to_seconds(exit_date) && error1==0)
                {
                    // error = Entry_Date_after_Exit_Date;
                    // Print_Error(error);
                    error2=1;
                }
                
                error3 = Invalid_Disease(virusName);
            } 

            //If country is given check its validity.
            if(strcmp(user_input[4],"0") != 0)
            {
                
                country = (char*)malloc((strlen(user_input[4])+1)*sizeof(char));
                strcpy(country,user_input[4]); 
            
                error4 = Invalid_Name(country);
            }
            
            //If all goes well.
            if(error == 0 && error1 == 0 && error2 == 0 && error3 == 0 && error4 == 0)
            {
                int a=0;
                //Find the number of outbreaks in [date1,date2] with virus name, such as the 1st Project.
                for(j=0;j<num_of_diseases;j++)
                {
                    char* result;
                    a = Num_of_Outbreaks(disease_array[j],entry_date,exit_date,virusName,country,&result);
                    if(a == 1)
                    {
                        Write_Socket(new_server_worker_socket,result);
                        free(result);
                        break;
                    }
                    //In case there is no such a disease 
                    if(j == (num_of_diseases-1) && a == 0)
                    {
                        Write_Socket(new_server_worker_socket,"ERROR");
                    }
                    // printf("There is no disease with name: %s.\n",virusName);
                }
            }
            else  
            {
                Write_Socket(new_server_worker_socket,"ERROR");
            }
            
            free(entry_date);
            free(exit_date);
            free(virusName);
            if(strcmp(user_input[4],"0") != 0)  free(country);
        }
        else if(strcmp(user_input[0],"/topk-AgeRanges") == 0)
        {   
            char* entry_date, *exit_date, *virusName, *kk, *country;
            int k,j, error=0,error1=0,error2=0,error3=0,error4=0,error5=0;

            //First of all I have to check the Validity of User Input

            kk = (char*)malloc((strlen(user_input[1])+1)*sizeof(char));
            strcpy(kk,user_input[1]);

            country = (char*)malloc((strlen(user_input[2])+1)*sizeof(char));
            strcpy(country,user_input[2]);

            virusName = (char*)malloc((strlen(user_input[3])+1)*sizeof(char));
            strcpy(virusName,user_input[3]);

            entry_date = (char*)malloc((strlen(user_input[4])+1)*sizeof(char));
            strcpy(entry_date,user_input[4]);

            exit_date = (char*)malloc((strlen(user_input[5])+1)*sizeof(char));
            strcpy(exit_date,user_input[5]);  

            error5 = is_number(kk);
            if(error5==0)
            {  
                k=atoi(kk);
                if(k<0 || k>9)  error5=-1;
            }

            error = Invalid_Date(entry_date);
            
            if(strcmp(user_input[3], "--") != 0)
            {
                error1 = Invalid_Date(exit_date);

                if(date_to_seconds(entry_date)>date_to_seconds(exit_date) && error1==0)
                {
                    // error = Entry_Date_after_Exit_Date;
                    // Print_Error(error);
                    error2=1;
                }
                
                error3 = Invalid_Disease(virusName);
            } 

            //If country is given check its validity.
            error4 = Invalid_Name(country);
            
            //If all goes well.
            if(error == 0 && error1 == 0 && error2 == 0 && error3 == 0 && error4 == 0 && error5 ==0)
            {
                char *msg=NULL;

                topk_AgeRanges(country_array, num_of_directories, k, country, virusName, entry_date, exit_date, &msg);

                // Write_Pipe(msg,pipe_C2P,bufferSize);  //Write to the parent the answer
                Write_Socket(new_server_worker_socket,msg);

                if(msg != NULL)  free(msg);
            }
            else
            {
                // Write_Pipe("ERROR",pipe_C2P,bufferSize);
                Write_Socket(new_server_worker_socket,"ERROR");
            }
            
            free(entry_date);
            free(exit_date);
            free(virusName);
            free(kk);
            free(country);
        }
        else if(strcmp(user_input[0],"/numPatientAdmissions") == 0)
        {   
            char* entry_date, *exit_date, *virusName, *country = NULL;
            int j, error=0,error1=0,error2=0,error3=0,error4=0;

            //First of all I have to check the Validity of User Input

            virusName = (char*)malloc((strlen(user_input[1])+1)*sizeof(char));
            strcpy(virusName,user_input[1]);

            entry_date = (char*)malloc((strlen(user_input[2])+1)*sizeof(char));
            strcpy(entry_date,user_input[2]);

            exit_date = (char*)malloc((strlen(user_input[3])+1)*sizeof(char));
            strcpy(exit_date,user_input[3]);  

            error = Invalid_Date(entry_date);
            
            if(strcmp(user_input[3], "--") != 0)
            {
                error1 = Invalid_Date(exit_date);

                if(date_to_seconds(entry_date)>date_to_seconds(exit_date) && error1==0)
                {
                    // error = Entry_Date_after_Exit_Date;
                    // Print_Error(error);
                    error2=1;
                }
                
                error3 = Invalid_Disease(virusName);
            } 

            //If country is given check its validity.
            if(strcmp(user_input[4],"0") != 0)
            {
                country = (char*)malloc((strlen(user_input[4])+1)*sizeof(char));
                strcpy(country,user_input[4]); 
            
                error4 = Invalid_Name(country);
            }
            
            //If all goes well.
            if(error == 0 && error1 == 0 && error2 == 0 && error3 == 0 && error4 == 0)
            {
                int a=0;
                //Find the number of outbreaks in [date1,date2] with virus name, such as the 1st Project.
                //Same as disease frequency, but now the result is accompanied by a specific country
                for(j=0;j<num_of_diseases;j++)
                {
                    char* result=NULL;
                    a = Num_Patient_Admissions(disease_array[j],entry_date,exit_date,virusName,country,&result);
                    
                    if(a == 1)
                    {
                        // Write_Pipe(result,pipe_C2P,bufferSize);
                        Write_Socket(new_server_worker_socket,result);
                        
                        free(result);
                        break;
                    }
                    //In case there is no such a disease 
                    if(j == (num_of_diseases-1) && a == 0)
                    {
                        // Write_Pipe("ERROR",pipe_C2P,bufferSize);
                        Write_Socket(new_server_worker_socket,"ERROR");
                    }
                        // printf("There is no disease with name: %s.\n",virusName);
                }
            }
            else
            { 
                // Write_Pipe("ERROR",pipe_C2P,bufferSize);
                Write_Socket(new_server_worker_socket,"ERROR");
            }
            
            free(entry_date);
            free(exit_date);
            free(virusName);
            if(strcmp(user_input[4],"0") != 0)  free(country);
        }
        else if(strcmp(user_input[0],"/numPatientDischarges") == 0)
        {   
            char* entry_date, *exit_date, *virusName, *country = NULL;
            int j, error=0,error1=0,error2=0,error3=0,error4=0;

            //First of all I have to check the Validity of User Input

            virusName = (char*)malloc((strlen(user_input[1])+1)*sizeof(char));
            strcpy(virusName,user_input[1]);

            entry_date = (char*)malloc((strlen(user_input[2])+1)*sizeof(char));
            strcpy(entry_date,user_input[2]);

            exit_date = (char*)malloc((strlen(user_input[3])+1)*sizeof(char));
            strcpy(exit_date,user_input[3]);  

            error = Invalid_Date(entry_date);
            
            if(strcmp(user_input[3], "--") != 0)
            {
                error1 = Invalid_Date(exit_date);

                if(date_to_seconds(entry_date)>date_to_seconds(exit_date) && error1==0)
                {
                    // error = Entry_Date_after_Exit_Date;
                    // Print_Error(error);
                    error2=1;
                }
                
                error3 = Invalid_Disease(virusName);
            } 

            //If country is given check its validity.
            if(strcmp(user_input[4],"0") != 0)
            {
                
                country = (char*)malloc((strlen(user_input[4])+1)*sizeof(char));
                strcpy(country,user_input[4]); 
            
                error4 = Invalid_Name(country);
            }
            
            //If all goes well.
            if(error == 0 && error1 == 0 && error2 == 0 && error3 == 0 && error4 == 0)
            {
                int a=0;
                for(j=0;j<num_of_diseases;j++)
                {
                    char* result=NULL;
                    //Find the number of people that are exited in [date1,date2] with virus name, such as the 1st Project
                    a = Num_Patient_Discharges(disease_array[j],entry_date,exit_date,virusName,country,&result);
                    if(a == 1)
                    {
                        // Write_Pipe(result,pipe_C2P,bufferSize);
                        Write_Socket(new_server_worker_socket,result);
                        
                        free(result);
                        break;
                    }
                    //In case there is no such a disease 
                    if(j == (num_of_diseases-1) && a == 0)
                    {
                        // Write_Pipe("ERROR",pipe_C2P,bufferSize);
                        Write_Socket(new_server_worker_socket,"ERROR");
                    }
                        // printf("There is no disease with name: %s.\n",virusName);
                }
            }
            else
            {  
                // Write_Pipe("ERROR",pipe_C2P,bufferSize);
                Write_Socket(new_server_worker_socket,"ERROR");
            }
            
            free(entry_date);
            free(exit_date);
            free(virusName);
            if(strcmp(user_input[4],"0") != 0)  free(country);
        }
        if(strcmp(user_input[0],"/searchPatientRecord")==0)
        {
            int error=0, success=0;
            char* search_record=NULL;
            char* record;
            rec_ptr current = head1;

            //First of all I have to check the Validity of User Input

            record = (char*)malloc((strlen(user_input[1])+1)*sizeof(char));
            strcpy(record,user_input[1]);

            error = Invalid_Record(record);

            //If the record is valid
            if(error == 0)
            {
                //Start search for the record in the list of records of the Worker has.
                while(current != NULL)
                {
                    //When he found it then save the whole record in an array and send it to the parent
                    if(strcmp(current->recordID,record)==0)
                    {
                        success=1;

                        search_record = (char*)malloc((strlen(current->recordID)+1)*sizeof(char));
                        strcpy(search_record,current->recordID);

                        search_record = (char*)realloc(search_record, (strlen(search_record)+1)+2);
                        strcat(search_record," ");

                        search_record = (char*)realloc(search_record, (strlen(search_record)+1 ) + (strlen(current->patientFirstName)+1));
                        strcat(search_record,current->patientFirstName); 

                        search_record = (char*)realloc(search_record, (strlen(search_record)+1)+2);
                        strcat(search_record," ");

                        search_record = (char*)realloc(search_record, (strlen(search_record)+1 ) + (strlen(current->patientLastName)+1));
                        strcat(search_record,current->patientLastName);

                        search_record = (char*)realloc(search_record, (strlen(search_record)+1)+2);
                        strcat(search_record," ");

                        search_record = (char*)realloc(search_record, (strlen(search_record)+1 ) + (strlen(current->diseaseID)+1));
                        strcat(search_record,current->diseaseID);

                        search_record = (char*)realloc(search_record, (strlen(search_record)+1)+2);
                        strcat(search_record," ");

                        search_record = (char*)realloc(search_record, (strlen(search_record)+1 ) + (strlen(current->age)+1));
                        strcat(search_record,current->age);

                        search_record = (char*)realloc(search_record, (strlen(search_record)+1)+2);
                        strcat(search_record," ");

                        search_record = (char*)realloc(search_record, (strlen(search_record)+1 ) + (strlen(current->entryDate)+1));
                        strcat(search_record,current->entryDate);

                        search_record = (char*)realloc(search_record, (strlen(search_record)+1)+2);
                        strcat(search_record," ");

                        search_record = (char*)realloc(search_record, (strlen(search_record)+1 ) + (strlen(current->exitDate)+1));
                        strcat(search_record,current->exitDate);

                        search_record = (char*)realloc(search_record, (strlen(search_record)+1)+3);
                        strcat(search_record,"\n");

                        // Write_Pipe(search_record,pipe_C2P,bufferSize);
                        Write_Socket(new_server_worker_socket,search_record);
                        free(search_record);
                    }
                    current = current->next;
                }

                //In case there is not such record with the given record id
                if(success == 0)
                {
                    // Write_Pipe("ERROR",pipe_C2P,bufferSize);
                    Write_Socket(new_server_worker_socket,"ERROR");
                }
            }
            else
            {
                // Write_Pipe("ERROR",pipe_C2P,bufferSize);
                Write_Socket(new_server_worker_socket,"ERROR");
            }
            
            free(record);
        }
        
        free(line); 
        close(new_server_worker_socket);
    }

    close(server_worker_socket);

    //Deallocate memory
    free(statistics);
    free(workerIP);
    
    for(j=0;j<(num_of_directories);j++)
        free(directories[j]);
    free(directories);

    for(j=0;j<(num_of_directories);j++)
        free(max_dates[j]);

    for(i=0;i<num_of_directories;i++)
    {
        for(j=0;j<num_of_date_files;j++)
            free(dates_array[i][j]);
        free(dates_array[i]);
    }
    free(dates_array);

    //Deallocate the memory of structs.
    Delete_Record_List(&head1);
    for(i=0; i<num_of_diseases; i++)
    {
        Destroy_Bucket_List(&disease_array[i]);
    }

    for(i=0; i<num_of_directories; i++)
    {
        Destroy_Bucket_List(&country_array[i]);
    }

    free(max_dates);
    return 0;
}
