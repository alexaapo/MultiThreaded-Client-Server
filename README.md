# MultiThreaded-Client-Server
The purpose of this Project III is to familiarize you with thread programming and network communication.
As part of this work I implemented a distributed process system that will provide the same
functionality with the diseaseAggregator application of the Project II. Specifically, I implemented three
programs: 1) a **master program** that will create a series of Worker processes (as did the parent process
in the second task), 2) a **multi-threaded whoServer** that will collect over the network summary statistics from Worker
processes and queries from clients, and 3) a **multithreaded client whoClient** program that will create
threads, that have the role of a client sending queries to whoServer. The architecture of this assignment is the following:


![Screenshot from 2020-09-22 13-24-57](https://user-images.githubusercontent.com/60033683/93871289-12b15080-fcd7-11ea-9438-d32a0a66ee8a.png)

### **A) The master program**
The master program will be used as follows:
**./master –w numWorkers -b bufferSize –s serverIP –p serverPort -i input_dir**  
where:  
- **numWorkers** parameter is the Worker number of processes that the application will generate.
- **bufferSize** parameter: is the size of the buffer for reading over pipes.
- **serverIP** parameter: is the IP address of the whoServer to which the Worker processes will connect to
send him summary statistics.
- **serverPort** parameter: is the port number where the whoServer listens.
- **input_dir** parameter: is a directory that contains subdirectories with the files to be processed
by the Workers.  

As in the Project II, each subdirectory will have the name of a country and will contain files with
names that are dates in the DD-MM-YYYY format. Each DD-MM-YYYY file will have exactly the same
format that he had in the second task and will contain a series of patient records where each line will describe
a patient who was entered/discharged to/from a hospital that day and contains the recordID, his name,
the virus, and its age.  
To begin, the master program starts numWorkers Workers child processes and share
uniformly the subdirectories with the countries in input_dir in Workers. Then, he starts the Workers and update each Worker through named pipe about the subdirectories that he will
undertaken. The parent process will send through named pipe both the IP address and the port number of whoServer.
When the creation of the Worker processes has finished, the parent process will remain (ie will not end) to
forks a new Worker process in case an existing Worker suddenly terminates.  
Each Worker process will read (for each directory) all its files in chronological order
of filenames and will update the data structures that they will use them to respond to
questions that whoServer will forward to him. It will connect to whoServer and send it the following information:  
1) a port number where the Worker process will listen for questions that will be forwarded by whoServer  
2) the summary statistics (same as in the Project II).  

When the Worker process finishes transferring information to whoServer,
will listen to the port number it has selected and wait for connections from whoServer for requests regarding
countries it manages.  

### **B) The whoServer program**
WhoServer will be used as follows:  
**./whoServer –q queryPortNum -s statisticsPortNum –w numThreads –b bufferSize**  
where:  
- **queryPortNum** parameter is a port number where whoServer will listen for query links
from the whoClient client
- **statisticsPortNum** parameter is a port number where whoServer will hear about connections to
summary statistics by Worker processes
- **numThreads** parameter: is the number of threads that whoServer will generate to serve
incoming connections from the network. Threads should be created once at the beginning when the
whoServer.
- **bufferSize** parameter is the size of a circular buffer to be shared between the threads that
are created by the whoServer process. The bufferSize represents the number of file/socket descriptors that
can be stored in it (eg 10, means 10 descriptors).  

When whoServer starts, the original thread should create numThreads threads. The
main (main process) thread will listen to the ports queryPortNum and statisticsPortNum, will accept connections
with the accept() system call and place the file/socket descriptors that
correspond to the connections in a circular buffer of size defined by bufferSize. The original thread will NOT
read from the connections it receives. Simply, whenever it accepts a connection it will place the file descriptor
which accept() returns to the buffer and will continue to accept subsequent connections. The work of numThreads
threads is to serve connections whose corresponding file descriptors are placed in
buffer. Each of the numThreads threads wakes up when there is at least one descriptor in the buffer.
More specifically, the original thread will listen to statisticsPortNum for connections from Worker processes to
receive the summary statistics and the port number where each Worker process listens, and will listen to
queryPortNum for connections from whoClient to receive queries about cases reported in
distributed system. 

whoServer will accept and serve the following requests from whoClient (similar to Project II):  
● /diseaseFrequency virusName date1 date2 [country]  
If no country argument is given, whoServer will find the number of cases for virusName
which have been recorded in the system within the space [date1 ... date2]. If a country argument is given, whoServer will find out about the virusName disease, the number of cases in the country that have
recorded in space [date1 ... date2]. The date1 date2 arguments will take the form
DD-MM-YYYY. The communication protocol between the client and whoServer should
manages in some way the fact that [country] is optional in this query.  


● /topk-AgeRanges k country disease date1 date2
WhoServer will find, for the country and the virus disease the top k age categories that have
cases of this virus in that country and their incidence rate. The
date1 date2 arguments will be in DD-MM-YYYY format.


● /searchPatientRecord recordID
WhoServer forwards the request to all Workers and waits for a response from the Worker with the record
recordID.


● /numPatientAdmissions disease date1 date2 [country]
If a country argument is given, whoServer should forward the request to workers in order to find the
total number of patients entered to hospital with the disease in that country within space [date1 date2]. If no country argument is given, he will find the number of patients with the disease
disease that entered the hospital in space [date1, date2].
The date1 date2 arguments will
have DD-MM-YYYY format.


● /numPatientDischarges disease date1 date2 [country]
If the country argument is given, whoServer will find the total number of patients with the disease
who have been discharged from a hospital in that country within [date1, date2]. If no
country argument is given, whoServer will find the number of patients with the disease who have been discharged from
hospital in space [date1, date2]. The date1, date2 arguments will be in DD-MM-YYYY format.  


When whoServer accepts a query, it forwards it to the corresponding worker processes through a socket and waits
the response from the workers. The question that is promoted in a Worker process along with the answers
whoServer receives from this Worker,I prints them to stdout. WhoServer also forwards the response to
corresponding whoClient thread that asked the query.

**Note**: I need to handle the problem writing several threads at the same time in stdout, so the output will not be confused.

### **C) The whoClient program**  
The whoClient program will be used as follows:  
**./whoClient –q queryFile -w numThreads –sp servPort –sip servIP**
- **queryFile** parameter is the file that contains the queries to be sent to whoServer.
- **numThreads** parameter: is the number of threads that whoClient will generate for sending
queries in whoServer
- **servPort** parameter is the port number where the whoServer listens to which the whoClient will connect.
- **servIP** parameter is the IP address of the whoServer to which the whoClient will connect.


The function of multithreaded whoClient is as follows: He starts by opening the queryFile file and
reads it line by line. In each line there will be one command from which whoServer can accept. For each
command it will be created a thread that will send a command (ie a line) to whoServer. Thread
will be created but will not connect to whoServer immediately. When all the threads are created, i.e we have a thread for each command in the file, then the threads should start all together to try to connect
to whoServer and send their command. When the command is sent, each thread will print the response received
from whoServer to stdout and can terminate. When all threads are terminated, the whoClient terminates as well.

## Execution:
1. **make**
2. 
- **make whoServer && ./whoServer -q 5000 -s 6000 -w 10 -b 5**
- **make Master && ./master -w 13 -b 1 -s 192.168.1.15 -p 6000 -i input_dir**
- **make whoClient && ./whoClient -q examples.txt -w 10 -sp 5000 -sip 192.168.1.15**
3. **make clean**

