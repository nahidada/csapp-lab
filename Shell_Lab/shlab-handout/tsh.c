/* 
 * tsh - A tiny shell program with job control
 * 
 * <Put your name and login ID here>
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

/* Misc manifest constants */
#define MAXLINE    1024   /* max line size */
#define MAXARGS     128   /* max args on a command line */
#define MAXJOBS      16   /* max jobs at any point in time */
#define MAXJID    1<<16   /* max job ID */

/* Job states */
#define UNDEF 0 /* undefined */
#define FG 1    /* running in foreground */
#define BG 2    /* running in background */
#define ST 3    /* stopped */

/* 
 * Jobs states: FG (foreground), BG (background), ST (stopped)
 * Job state transitions and enabling actions:
 *     FG -> ST  : ctrl-z
 *     ST -> FG  : fg command
 *     ST -> BG  : bg command
 *     BG -> FG  : fg command
 * At most 1 job can be in the FG state.
 */

/* Global variables */
extern char **environ;      /* defined in libc */
char prompt[] = "tsh> ";    /* command line prompt (DO NOT CHANGE) */
int verbose = 0;            /* if true, print additional output */
int nextjid = 1;            /* next job ID to allocate */
char sbuf[MAXLINE];         /* for composing sprintf messages */

struct job_t {              /* The job struct */
    pid_t pid;              /* job PID */
    int jid;                /* job ID [1, 2, ...] */
    int state;              /* UNDEF, BG, FG, or ST */
    char cmdline[MAXLINE];  /* command line */
};
struct job_t jobs[MAXJOBS]; /* The job list */
/* End global variables */

/* Function prototypes */

/* Here are the functions that you will implement */
void eval(char *cmdline);
int builtin_cmd(char **argv);
void do_bgfg(char **argv);
void waitfg(pid_t pid);

void sigchld_handler(int sig);
void sigtstp_handler(int sig);
void sigint_handler(int sig);

/* Here are helper routines that we've provided for you */
int parseline(const char *cmdline, char **argv); 
void sigquit_handler(int sig);

void clearjob(struct job_t *job);
void initjobs(struct job_t *jobs);
int maxjid(struct job_t *jobs); 
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline);
int deletejob(struct job_t *jobs, pid_t pid); 
pid_t fgpid(struct job_t *jobs);
struct job_t *getjobpid(struct job_t *jobs, pid_t pid);
struct job_t *getjobjid(struct job_t *jobs, int jid); 
int pid2jid(pid_t pid); 
void listjobs(struct job_t *jobs);

void usage(void);
void unix_error(char *msg);
void app_error(char *msg);
typedef void handler_t(int);
handler_t *Signal(int signum, handler_t *handler);

/* system call error handling */
pid_t fork_error_handling(void);

pid_t fork_error_handling(void){
    
    pid_t pid;

    if((pid= fork()) < 0){
        unix_error("Fork error");
    }
    
    return pid;
}

void kill_error_handling(pid_t pid, int sig){
   
    if(kill(pid, sig) < 0){
        unix_error("Kill error");
    }
    return;
}


/*
 * main - The shell's main routine 
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
    while ((c = getopt(argc, argv, "hvp")) != EOF) {
        switch (c) {
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
    while (1) {

        /* Read command line */
        if (emit_prompt) {
            printf("%s", prompt);
            fflush(stdout);
        }
        if ((fgets(cmdline, MAXLINE, stdin) == NULL) && ferror(stdin))
            app_error("fgets error");
        if (feof(stdin)) { /* End of file (ctrl-d) */
            fflush(stdout);
            exit(0);
        }

        // printf("You entered: %s \n", cmdline);

        /* Evaluate the command line */
        eval(cmdline);
        fflush(stdout);
        fflush(stdout);
    } 

    exit(0); /* control never reaches here */
}
  
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
    char* argv[MAXARGS];
    int bg;
    pid_t pid;

    bg = parseline(cmdline, argv);
    if(argv[0] == NULL){
        printf("empty cmdline \n");
        //empty cmdline
        return;
    }



    if(builtin_cmd(argv) == 0){
        //not built in cmd

        sigset_t mask_chld;
        sigaddset(&mask_chld, SIGCHLD);
        // BLOCK SIGCHLD
        sigprocmask(SIG_BLOCK, &mask_chld, NULL);

        //executable program
        if((pid=fork_error_handling()) == 0){
            setpgid(0,0);
            //child run cmd
            // UNBLOCK SIGCHLD
            sigprocmask(SIG_UNBLOCK, &mask_chld, NULL);

            if(execve(argv[0],argv, environ) < 0){
                printf("%s cmd not found. \n", argv[0]);
                exit(1);
            }

        }  
        
        //parent waits for foreground job to terminate
        if(bg){
            //bg
            //update pgid, ignore
            addjob(jobs, pid, BG, cmdline);
            // UNBLOCK SIGCHLD
            sigprocmask(SIG_UNBLOCK, &mask_chld, NULL);
            printf("[%d] (%d) %s", pid2jid(pid), (int)pid, cmdline); 

        }else{
            //fg
            //update pgid, ignore
            addjob(jobs, pid, FG, cmdline);
            // UNBLOCK SIGCHLD
            sigprocmask(SIG_UNBLOCK, &mask_chld, NULL);
            waitfg(pid);

        }
    }

   return;
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

    // printf("parse line : %s \n", cmdline);

    strcpy(buf, cmdline);
    buf[strlen(buf)-1] = ' ';  /* replace trailing '\n' with space */
    while (*buf && (*buf == ' ')) /* ignore leading spaces */
	buf++;

    /* Build the argv list */
    argc = 0;
    if (*buf == '\'') {
	buf++;
	delim = strchr(buf, '\'');
    }
    else {
	delim = strchr(buf, ' ');
    // printf("parse line delim : %s \n", delim);
    }

    while (delim) {
	argv[argc++] = buf;
	*delim = '\0';
	buf = delim + 1;
	while (*buf && (*buf == ' ')) /* ignore spaces */
	       buf++;

	if (*buf == '\'') {
	    buf++;
	    delim = strchr(buf, '\'');
	}
	else {
	    delim = strchr(buf, ' ');
	}
    }
    argv[argc] = NULL;
    
    if (argc == 0)  /* ignore blank line */
	return 1;

    /* should the job run in the background? */
    if ((bg = (*argv[argc-1] == '&')) != 0) {
	argv[--argc] = NULL;
    }
    return bg;
}

/* 
 * builtin_cmd - If the user has typed a built-in command then execute
 *    it immediately.  
 */
int builtin_cmd(char **argv) 
{
    char* cmd = argv[0];

    if(strcmp(cmd, "quit") == 0){
        exit(0);
    }

    if(strcmp(cmd, "jobs") == 0){
        listjobs(jobs);
        return 1;
    }

    if(strcmp(cmd, "bg") == 0){
        if(verbose){
            int i;
            for( i=0; i<MAXARGS; i++){
                if( argv[i] != NULL ){
                    printf("eval argv %d : %s \n", i, argv[i]);
                }else{
                    break;
                }
            }
        }
        do_bgfg(argv);
        return 1;
    }

    if(strcmp(cmd, "fg") == 0){
        do_bgfg(argv);
        return 1;
    }

    return 0;     /* not a builtin command */
}

/* 
 * do_bgfg - Execute the builtin bg and fg commands
 * e.g.
 * bg %jobid or bg pid
 */
void do_bgfg(char **argv){
    if(argv[1] == NULL){
        printf("%s cmd requires pid or job id. \n", argv[0]);
        return;
    }

    if(argv[1][0] != '%' && !isdigit(argv[1][0])){
        if(verbose){
            printf("argv[1][0] %c \n", argv[1][0]);
        }
        
        printf("%s cmd must be pid or job id. \n", argv[0]);
        return;
    }

    int is_job_id = (argv[1][0] == '%' ? 1:0);
    struct job_t * tmp_job;
    
    if(is_job_id){
        // bg or fg %(jid)
        tmp_job = getjobjid(jobs, atoi(&argv[1][1]));
        if(tmp_job == NULL){
            printf("No job: %s \n", argv[1]);
            return;
        }

    }else{
        // bg or fg pid
        tmp_job = getjobpid(jobs, atoi(argv[1]));
        if(tmp_job == NULL){
            printf("No process: %s \n", argv[1]);
            return;
        }
    }

    if(strcmp(argv[0], "bg") == 0){
        /*
        ST -> BG  : bg command
        */ 
        tmp_job->state = BG;
        printf("[%d] (%d) %s", tmp_job->jid, tmp_job->pid, tmp_job->cmdline);
        kill_error_handling(-tmp_job->pid, SIGCONT);

    }else{
        /* 
        ST -> FG  : fg command
        BG -> FG  : fg command 
        */
        tmp_job->state = FG;
        kill_error_handling(-tmp_job->pid, SIGCONT);
        waitfg(tmp_job->pid);
    }
    return;
}

/* 
 * waitfg - Block until process pid is no longer the foreground process
 */
void waitfg(pid_t pid)
{
    while(1){
        pid_t fg_pid = fgpid(jobs);

        if(pid != fg_pid){
            //  printf("waitfg: Process (%d) no longer the fg process (%d)\n", (int) pid, (int)fg_pid);
            if (verbose)
                printf("waitfg: Process (%d) no longer the fg process\n", (int) pid);
            break;
        }else{
            sleep(1);
        }
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
    if(verbose){
        printf("sigchld_handler: entering\n");
    }

    pid_t chld_pid;
    int status;
    int jobid;

    // bg or fg can send sigchld to parent
    while ((chld_pid =waitpid(-1, &status, WNOHANG | WUNTRACED)) > 0)
    {
        jobid = pid2jid(chld_pid);

        if(WIFEXITED(status)){
            //normally terminated ,via return or exit
            deletejob(jobs, chld_pid);
            if (verbose)
                printf("sigchld_handler: Job [%d] (%d) deleted\n", jobid, (int) chld_pid);
            if (verbose)
                printf("sigchld_handler: Job [%d] (%d) terminates OK (status %d)\n", jobid, (int) chld_pid, WEXITSTATUS(status));
        }

        // WIFSIGNALED returns true if the child process terminated because of a signal that was not caught
        // For example, SIGTERM default behavior is terminate
        if(WIFSIGNALED(status)){
            deletejob(jobs, chld_pid);

            if (verbose)
                printf("sigchld_handler: Job [%d] (%d) deleted\n", jobid, (int) chld_pid);
            printf("Job [%d] (%d) terminated by signal %d\n", jobid, (int) chld_pid, WTERMSIG(status));
        }

        if(WIFSTOPPED(status)){
            
            struct job_t *cur_stop_job = getjobjid(jobs, jobid);
            
            if(cur_stop_job != NULL){
                cur_stop_job->state = ST;
                printf("Job [%d] (%d) stopped by signal %d\n", jobid, (int) chld_pid, WSTOPSIG(status));
            }else{
                printf("can not find jobid %d\n",jobid);
            }
        }

        if (verbose)
            printf("sigchld_handler: exiting\n");
        return;
    }
    
    if(verbose){
        printf("sigchld_handler: exiting\n");
    } 

    return;
}

/* 
 * sigint_handler - The kernel sends a SIGINT to the shell whenver the
 *    user types ctrl-c at the keyboard.  Catch it and send it along
 *    to the foreground job.  
 */
void sigint_handler(int sig) 
{
    if(verbose){
        printf("sigint_handler: entering\n");
    }
    
    pid_t fg_pid = fgpid(jobs);
    if(fg_pid != 0){
        kill_error_handling(-fg_pid, sig);
        if(verbose){
            printf("sigint_handler: pid [%d] and its entire foreground jobs with same process group are killed\n", (int)fg_pid);
        }
    }

    if(verbose){
        printf("sigint_handler: exiting\n");
    }
    
    return;
}

/*
 * sigtstp_handler - The kernel sends a SIGTSTP to the shell whenever
 *     the user types ctrl-z at the keyboard. Catch it and suspend the
 *     foreground job by sending it a SIGTSTP.  
 */
void sigtstp_handler(int sig) 
{
    if(verbose){
        printf("sigtstp_handler: entering\n");
    }
    
    pid_t fg_pid = fgpid(jobs);
    
    if(fg_pid != 0){
        
        kill_error_handling(-fg_pid, sig);

        if(verbose){
            printf("sigtstp_handler: pid [%d] and its entire foreground jobs with same process group are killed\n", (int)fg_pid);
        }
    }
    
    if(verbose){
        printf("sigtstp_handler: exiting\n");
    }
    
    return;

}

/*********************
 * End signal handlers
 *********************/

/***********************************************
 * Helper routines that manipulate the job list
 **********************************************/

/* clearjob - Clear the entries in a job struct */
void clearjob(struct job_t *job) {
    job->pid = 0;
    job->jid = 0;
    job->state = UNDEF;
    job->cmdline[0] = '\0';
}

/* initjobs - Initialize the job list */
void initjobs(struct job_t *jobs) {
    int i;

    for (i = 0; i < MAXJOBS; i++)
	clearjob(&jobs[i]);
}

/* maxjid - Returns largest allocated job ID */
int maxjid(struct job_t *jobs) 
{
    int i, max=0;

    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].jid > max)
	    max = jobs[i].jid;
    return max;
}

/* addjob - Add a job to the job list */
int addjob(struct job_t *jobs, pid_t pid, int state, char *cmdline) 
{
    int i;
    
    if (pid < 1)
	return 0;

    for (i = 0; i < MAXJOBS; i++) {
	if (jobs[i].pid == 0) {
	    jobs[i].pid = pid;
	    jobs[i].state = state;
	    jobs[i].jid = nextjid++;
	    if (nextjid > MAXJOBS)
		nextjid = 1;
	    strcpy(jobs[i].cmdline, cmdline);
        // printf("Added job [%d] %d %d %s\n", jobs[i].jid, jobs[i].pid, jobs[i].state,jobs[i].cmdline);
  	    if(verbose){
	        printf("Added job [%d] %d %s\n", jobs[i].jid, jobs[i].pid, jobs[i].cmdline);
        }
            return 1;
	}
    }
    printf("Tried to create too many jobs\n");
    return 0;
}

/* deletejob - Delete a job whose PID=pid from the job list */
int deletejob(struct job_t *jobs, pid_t pid) 
{
    int i;
    
    if(verbose){
        printf("delete pid %d",(int) pid);
    }
    
    if (pid < 1)
	    return 0;

    for (i = 0; i < MAXJOBS; i++) {
        if (jobs[i].pid == pid) {
            // printf("delete job pid %d \n", (int)pid);
            clearjob(&jobs[i]);
            nextjid = maxjid(jobs)+1;
            return 1;
        }
    }
    return 0;
}

/* fgpid - Return PID of current foreground job, 0 if no such job */
pid_t fgpid(struct job_t *jobs) {
    int i;

    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].state == FG)
	    return jobs[i].pid;
    return 0;
}

/* getjobpid  - Find a job (by PID) on the job list */
struct job_t *getjobpid(struct job_t *jobs, pid_t pid) {
    int i;

    if (pid < 1)
	return NULL;
    for (i = 0; i < MAXJOBS; i++)
	if (jobs[i].pid == pid)
	    return &jobs[i];
    return NULL;
}

/* getjobjid  - Find a job (by JID) on the job list */
struct job_t *getjobjid(struct job_t *jobs, int jid) 
{
    int i;

    if (jid < 1){
        if(verbose){
            printf("job < 1 \n");
        }
        return NULL;
    }
	    
    for (i = 0; i < MAXJOBS; i++){
        if (jobs[i].jid == jid){
            if(verbose){
                printf("return %p \n", (void *)&jobs[i]);
            }
            return &jobs[i];
        }  
    }

    if(verbose){
        printf("can not find jobid : %d \n",jid);
    }
    return NULL;
}

/* pid2jid - Map process ID to job ID */
int pid2jid(pid_t pid) 
{
    int i;

    if (pid < 1)
	    return 0;
    for (i = 0; i < MAXJOBS; i++)
	    if (jobs[i].pid == pid) {
            return jobs[i].jid;
        }
    return 0;
}

/* listjobs - Print the job list */
void listjobs(struct job_t *jobs) 
{
    int i;
    
    for (i = 0; i < MAXJOBS; i++) {
	if (jobs[i].pid != 0) {
	    printf("[%d] (%d) ", jobs[i].jid, jobs[i].pid);
	    switch (jobs[i].state) {
		case BG: 
		    printf("Running ");
		    break;
		case FG: 
		    printf("Foreground ");
		    break;
		case ST: 
		    printf("Stopped ");
		    break;
	    default:
		    printf("listjobs: Internal error: job[%d].state=%d ", 
			   i, jobs[i].state);
	    }
	    printf("%s", jobs[i].cmdline);
	}
    }
}
/******************************
 * end job list helper routines
 ******************************/


/***********************
 * Other helper routines
 ***********************/

/*
 * usage - print a help message
 */
void usage(void) 
{
    printf("Usage: shell [-hvp]\n");
    printf("   -h   print this message\n");
    printf("   -v   print additional diagnostic information\n");
    printf("   -p   do not emit a command prompt\n");
    exit(1);
}

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

/*
 * sigquit_handler - The driver program can gracefully terminate the
 *    child shell by sending it a SIGQUIT signal.
 */
void sigquit_handler(int sig) 
{
    printf("Terminating after receipt of SIGQUIT signal\n");
    exit(1);
}



