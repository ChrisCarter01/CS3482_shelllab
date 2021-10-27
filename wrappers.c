#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include "wrappers.h"

#include <unistd.h>
#include <sys/types.h>

//Add the missing wrappers to this file (for fork, exec, sigprocmask, etc.)
//Note that exec does not return a value (void function). If an error
//occurs, it will simply output <command>: Command not found, 
//where <command> is the value of argv[0] and then call exit. (See code in figure 8.24.)
//The other wrappers will be in the style of the Fork wrapper in section 8.3. 

/*
 * unix_error - unix-style error routine
 */
void unix_error(char *msg)
{
    fprintf(stdout, "%s: %s\n", msg, strerror(errno));
    exit(1);
}

/*
 * app_error - application-style error routine
 */
void app_error(char *msg)
{
    fprintf(stdout, "%s\n", msg);
    exit(1);
}

/*
 * Signal - wrapper for the sigaction function
 */
handler_t *Signal(int signum, handler_t *handler) 
{
    struct sigaction action, old_action;

    action.sa_handler = handler;  
    sigemptyset(&action.sa_mask); /* block sigs of type being handled */
    action.sa_flags = SA_RESTART; /* restart syscalls if possible */

    if (sigaction(signum, &action, &old_action) < 0)
	unix_error("Signal error");
    return (old_action.sa_handler);
}

int Fork() {
    pid_t pid;
    pid = fork();
    if (pid < 0) {
         unix_error("Fork Error");
         exit(0);
    }
    return pid;
}

void Exec(char *cmdline, char** argv) {
    if (execv(cmdline, argv) < 0) {
        printf("%s: Command not found", argv[0]);
        exit(0);
    }
}

void Sigprocmask(int how, const sigset_t *set, sigset_t *oldset) {
    if(sigprocmask(how, set, oldset) < 0) {
        unix_error("Sigprocmask Error");
    }
}

void SigFillSet(sigset_t* set) {
    if(sigfillset(set) < 0) {
        unix_error("SigFillSet Error");
    }
}

void SigEmptySet(sigset_t* set) {
    if(sigemptyset(set) < 0) {
        unix_error("SigEmptySet Error");
    }
}

void SigAddSet(sigset_t* set, int signal) {
    if(sigaddset(set, signal) < 0) {
        unix_error("SigAddSet Error");
    }
}