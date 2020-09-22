#include <stdio.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include "handler_server.h"

extern volatile sig_atomic_t flag_server;
extern volatile sig_atomic_t worker_pid;

void Handler_Server(int sig, siginfo_t *siginfo, void *context)
{
    flag_server=1;
}

void Handler_New_Worker_Server(int sig, siginfo_t *siginfo, void *context)
{
    worker_pid = (long)siginfo->si_pid;
}