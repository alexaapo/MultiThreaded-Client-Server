#ifndef HANDLER_SERVER_H
#define HANDLER_SERVER_H
#include <signal.h>

void Handler_Server(int, siginfo_t *, void *);
void Handler_New_Worker_Server(int, siginfo_t *, void *);

#endif