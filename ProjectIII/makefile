default: Master whoClient whoServer
Master: worker master
whoServer: who_server.o who_server_functions.o handler_server.o
	gcc -pthread who_server.o who_server_functions.o handler_server.o -o whoServer
whoClient: who_client.o who_client_functions.o
	gcc -pthread who_client.o who_client_functions.o -o whoClient 
master: master.o read_write.o handler.o bucket_list.o hash.o avl.o max_heap.o record.o statistics.o
	gcc master.o read_write.o handler.o bucket_list.o hash.o avl.o max_heap.o record.o statistics.o -o master
worker: worker.o avl.o record.o bucket_list.o max_heap.o list.o hash.o statistics.o read_write.o handler_worker.o
	gcc worker.o avl.o record.o bucket_list.o max_heap.o list.o hash.o statistics.o read_write.o handler_worker.o -o worker 
who_server.o:
	gcc -c -o who_server.o who_server.c
who_server_functions.o:
	gcc -c -o who_server_functions.o who_server_functions.c
who_client.o:
	gcc -c -o who_client.o who_client.c
who_client_functions.o:
	gcc -c -o who_client_functions.o who_client_functions.c
master.o:
	gcc -c -o master.o master.c
worker.o:
	gcc -c -o worker.o worker.c
handler_worker.o:
	gcc -c -o handler_worker.o handler_worker.c
handler.o:
	gcc -c -o handler.o handler.c
handler_server.o:
	gcc -c -o handler_server.o handler_server.c
read_write.o:
	gcc -c -o read_write.o read_write.c
statistics.o:
	gcc -c -o statistics.o statistics.c
hash.o:
	gcc -c -o hash.o hash.c
list.o:
	gcc -c -o list.o list.c
record.o: 
	gcc -c -o record.o record.c
bucket_list.o: 
	gcc -c -o bucket_list.o bucket_list.c
avl.o: 
	gcc -c -o avl.o avl.c
max_heap.c: 
	gcc -c -o max_heap.o max_heap.c
clean:
	rm worker.o avl.o record.o master.o bucket_list.o max_heap.o list.o hash.o statistics.o read_write.o handler_worker.o handler.o handler_server.o who_server.o who_server_functions.o who_client.o who_client_functions.o worker master whoServer whoClient Pipe_* log_file.*
