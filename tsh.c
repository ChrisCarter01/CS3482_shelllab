/* 
 * tsh - A tiny shell program with job control
 * 
 * <Put your name(s) and login ID(s) here>
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include "jobs.h"      //prototypes for functions that manage the jobs list
#include "wrappers.h"  //prototypes for functions in wrappers.c

#include <signal.h> 

/* Misc manifest constants */
#define MAXLINE    1024   /* max line size */
#define MAXARGS     128   /* max args on a command line */

/* Global variables */
extern char **environ;      /* defined in libc */
char prompt[] = "tsh> ";    /* command line prompt (DO NOT CHANGE) */
int verbose = 0;            /* if true, print additional output (-v option) */
char sbuf[MAXLINE];         /* for composing sprintf messages */

/* Here are the prototypes for the functions that you will 
 * implement in this file.
 */
void eval(char *cmdline);
int builtin_cmd(char **argv);
void do_bgfg(char **argv);
void waitfg(pid_t pid);
void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);

/* Routines in this file that are already written */
int parseline(const char *cmdline, char **argv); 
void sigquit_handler(int sig);
void usage(void);

/*
 * main - The shell's main routine 
 *        The main is complete. You have nothing to do here.
 */
int main(int argc, char **argv) 
{
    char c;
    char cmdline[MAXLINE];
    int emit_prompt = 1; /* emit prompt (default) */

    /* Redirect stderr to stdout (so that driver will get all output
     * on the pipe connected to stdout) */
    dup2(1, 2);

    /* Parse the command line */
    while ((c = getopt(argc, argv, "hvp")) != EOF) 
    {
        switch (c) 
        {
            case 'h':             /* print help message */
                usage();
                break;
            case 'v':             /* emit additional diagnostic info */
                verbose = 1;
                break;
            case 'p':             /* don't print a prompt */
                emit_prompt = 0;  /* handy for automatic testing */
                break;
            default:
                usage();
        }
    }

    /* Install the signal handlers */

    /* These are the ones you will need to implement */
    Signal(SIGINT,  sigint_handler);   /* ctrl-c */
    Signal(SIGTSTP, sigtstp_handler);  /* ctrl-z */
    Signal(SIGCHLD, sigchld_handler);  /* Terminated or stopped child */

    /* This one provides a clean way to kill the shell */
    Signal(SIGQUIT, sigquit_handler); 

    /* Initialize the job list */
    initjobs(jobs);

    /* Execute the shell's read/eval loop */
    while (1) 
    {

        /* Read command line */
        if (emit_prompt) 
        {
            printf("%s", prompt);
            fflush(stdout);
        }
        if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin))
            app_error("fgets error");
        if (feof(stdin)) 
        { /* End of file (ctrl-d) */
            fflush(stdout);
            exit(0);
        }

        /* Evaluate the command line */
        eval(cmdline);
        fflush(stdout);
        fflush(stdout);
    } 

    exit(0); /* control never reaches here */
}

/* TODO:
 * Your work involves writing the functions below.  Plus, you'll need
 * to implement wrappers for system calls (Fork, Exec, etc.) Those wrappers
 * will go in wrappers.c.  You'll put prototypes for those in wrappers.h.
 *
 * Do NOT just start implementing the functions below.  You will do trace file
 * by trace file development by following the steps in the lab.
 * Seriously, do not do this any other way. You will be sorry.
 *
 * For many of these functions, you'll need to call functions in the jobs.c
 * file.  Be sure to take a look at those.
 */
  
/* 
 * eval - Evaluate the command line that the user has just typed in
 * 
 * If the user has requested a built-in command (quit, jobs, bg or fg)
 * then execute it immediately. Otherwise, fork a child process and
 * run the job in the context of the child. If the job is running in
 * the foreground, wait for it to terminate and then return.  Note:
 * each child process must have a unique process group ID so that our
 * background children don't receive SIGINT (SIGTSTP) from the kernel
 * when we type ctrl-c (ctrl-z) at the keyboard.  
*/
void eval(char *cmdline) 
{   
    sigset_t all_set, one_set, last_set;
    SigFillSet(&all_set);
    SigEmptySet(&one_set);
    SigAddSet(&one_set, SIGCHLD);

    //Declare an array of char pointers to hold arguements.
    char* argv[MAXARGS];
    char buffer[MAXLINE];
    int bg = 0;
    pid_t pid;

    strcpy(buffer, cmdline);
    bg = parseline(buffer, argv);
    //If arg0 is null then the line is empty.
    if (argv[0] == NULL) return;

    //If the builtin cmd function returns zero, then the given command is NOT built in.
    if(builtin_cmd(argv) == 0) {
        Sigprocmask(SIG_BLOCK, &one_set, &last_set);    //Block sig child
        
        if ((pid = Fork()) == 0) {  
            //This section of code will be executed by the child process.
            setpgid(0,0);   //Change the program group id for the child, so that SIGINT will be handled correctly.
            Sigprocmask(SIG_SETMASK, &last_set, NULL);  //Restore previous mask for the child
            Exec(argv[0], argv);
        }
        //Since the child process calls exit(), this section of code will only be excuted by the parent process.
        Sigprocmask(SIG_BLOCK, &all_set, NULL);     //Block all signals while adding to the job list.
        //Add jobs to job list.
        if (!bg) addjob(jobs, pid, FG, cmdline);
        else {
            addjob(jobs, pid, BG, cmdline);
            printf("[%d] (%d) %s", pid2jid(pid), pid, cmdline);
        }
        
        Sigprocmask(SIG_SETMASK, &last_set, NULL);  //Restore the original mask to the parent.
        //If BG is false, than the parent needs to wait for the child to finish processing.
        if (!bg) {
            waitfg(pid);
        }
    }
    return; 
}

/* 
 * builtin_cmd - If the user has typed a built-in command then execute
 *    it immediately.  
 */
int builtin_cmd(char **argv) 
{
    //If argv[0] is equal to 0, then quit the shell.
    if (!(strcmp(argv[0], "quit"))) exit(0);
    if (!(strcmp(argv[0], "jobs"))) {
        listjobs(jobs);
        return 1;
    }
    if (!(strcmp(argv[0], "bg"))) {
        do_bgfg(argv);
        return 1;
    }
    if (!(strcmp(argv[0], "fg"))) {
        do_bgfg(argv);
        return 1;
    }

    return 0;     /* not a builtin command */
}

/* 
 * do_bgfg - Execute the builtin bg and fg commands
 */
void do_bgfg(char **argv) 
{
    pid_t pid;
    //test if the first charcter of argv[1] is a % sign
    if(argv[1][0] != '%') {
        //The first arguement corresponds to the PID of the desired process
        pid = atoi(argv[1]);
    } else {
        //The first arguement corresponds to the job number of the desired process
        int jobid = atoi(argv[1] + 1);
        pid = getjobjid(jobs, jobid)->pid;
    }
    // printf("Found PID = %d\n", pid);
    if (!(strcmp(argv[0], "bg"))) {
        Kill(pid, SIGCONT);
        job_t* job = getjobpid(jobs, pid);
        printf("[%d] (%d) %s", job->jid, job->pid, job->cmdline);
        job->state = BG;
    }
    if (!(strcmp(argv[0], "fg"))) {
        Killpg(pid, SIGCONT);
        job_t* job = getjobpid(jobs, pid);
        job->state = FG;
        waitfg(pid);
    }
    return;
}

/* 
 * waitfg - Block until process pid is no longer the foreground process
 */
void waitfg(pid_t pid)
{
    while(fgpid(jobs) == pid && getjobpid(jobs, pid)->state == FG) {
        sleep(5);
    }
    return;
}

/*****************
 * Signal handlers
 *****************/

/* 
 * sigchld_handler - The kernel sends a SIGCHLD to the shell whenever
 *     a child job terminates (becomes a zombie), or stops because it
 *     received a SIGSTOP or SIGTSTP signal. The handler reaps all
 *     available zombie children, but doesn't wait for any other
 *     currently running children to terminate.  
 */
void sigchld_handler(int sig) 
{
    int status;
    sigset_t all_set, last_set;
    SigFillSet(&all_set);
    pid_t pid = 1;

    while(pid > 0) {
        pid = waitpid(-1, &status, WNOHANG | WUNTRACED);
        
        // printf("PID: %d\nStatus: %d\n", pid, status);
        if(WIFEXITED(status)) {
            Sigprocmask(SIG_BLOCK, &all_set, &last_set);    //Block all signals
            //Check if the child terminated normally.
            //Remove the child from the job list.
            deletejob(jobs, pid);
            Sigprocmask(SIG_SETMASK, &last_set, NULL);    //Unblock all signals
            
        }
        if(WIFSIGNALED(status)) {
            Sigprocmask(SIG_BLOCK, &all_set, &last_set);    //Block all signals
            //Check if child terminated abnormally (due to SIGINT or other signal).
            job_t job = *getjobpid(jobs, pid);
            printf("Job [%d] (%d) terminated by signal %d\n", job.jid, job.pid, status);
            deletejob(jobs, job.pid);
            status = 0; //Reset status so this code block will not repeat itself.
            
            Sigprocmask(SIG_SETMASK, &last_set, NULL);    //Unblock all signals
        }
        if(WIFSTOPPED(status)) {
            Sigprocmask(SIG_BLOCK, &all_set, &last_set);    //Block all signals
            //Check if child terminated abnormally (due to SIGINT or other signal).
            job_t* job = getjobpid(jobs, pid);
            printf("Job [%d] (%d) stopped by signal %d\n", job->jid, job->pid, WSTOPSIG(status));
            job->state = ST;
            status = 0; //Reset status so this code block will not repeat itself.
            
            Sigprocmask(SIG_SETMASK, &last_set, NULL);    //Unblock all signals
        }
    }
}

/* 
 * sigint_handler - The kernel sends a SIGINT to the shell whenver the
 *    user types ctrl-c at the keyboard.  Catch it and send it along
 *    to the foreground job.  
 */
void sigint_handler(int sig) 
{
    // printf("Handling SIGINT\n");
    pid_t fg = fgpid(jobs);
    Killpg(fg, SIGINT);
    return;
}

/*
 * sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
 *     the user types ctrl-z at the keyboard. Catch it and suspend the
 *     foreground job by sending it a SIGTSTP.  
 */
void sigtstp_handler(int sig) 
{
    pid_t fg = fgpid(jobs);
    Killpg(fg, SIGTSTP);
    return;
}

/*************************************
 *  The rest of these are complete.
 *************************************/


/*
 * sigquit_handler - The driver program can gracefully terminate the
 *    child shell by sending it a SIGQUIT signal.
 */
void sigquit_handler(int sig)
{
    printf("Terminating after receipt of SIGQUIT signal\n");
    exit(1);
}

/* 
 * parseline - Parse the command line and build the argv array.
 * 
 * Characters enclosed in single quotes are treated as a single
 * argument.  Return true if the user has requested a BG job, false if
 * the user has requested a FG job.  
 */
int parseline(const char *cmdline, char **argv)
{
    static char array[MAXLINE]; /* holds local copy of command line */
    char *buf = array;          /* ptr that traverses command line */
    char *delim;                /* points to first space delimiter */
    int argc;                   /* number of args */
    int bg;                     /* background job? */

    strcpy(buf, cmdline);
    buf[strlen(buf)-1] = ' ';  /* replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) buf++; /* ignore leading spaces */

    /* Build the argv list */
    argc = 0;
    if (*buf == '\'')
    {
        buf++;
        delim = strchr(buf, '\'');
    }
    else
    {
        delim = strchr(buf, ' ');
    }
    while (delim)
    {
        argv[argc++] = buf;
        *delim = '\0';
        buf = delim + 1;
        while (*buf && (*buf == ' ')) buf++; /* ignore spaces */

        if (*buf == '\'')
        {
            buf++;
            delim = strchr(buf, '\'');
        }
        else
        {
            delim = strchr(buf, ' ');
        }
    }
    argv[argc] = NULL;

    if (argc == 0)  return 1; /* ignore blank line */

    /* should the job run in the background? */
    if ((bg = (*argv[argc-1] == '&')) != 0)
    {
        argv[--argc] = NULL;
    }
    return bg;
}


/*
 * usage - print a help message
 */
void usage(void) 
{
    printf("Usage: shell [-hvp]\n");
    printf("   -h   print this message\n");
    //-v enables verbose
    printf("   -v   print additional diagnostic information\n");
    //the tester uses the -p option
    printf("   -p   do not emit a command prompt\n");
    exit(1);
}

