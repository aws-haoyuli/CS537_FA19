// User scheduler program
#include "types.h"
#include "x86.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"
#include "mmu.h"
#include "traps.h"
#include "pstat.h"

// Returns string representation of enum value for printing.
const char *get_enum_string(enum procstate state);

int main(int argc, char *argv[]) {
    if (argc != 5) {
        printf(1, "userRR usage: userRR <user-level-timeslice> <iterations> <job> <jobcount>\n");
        exit();
    }
    
    // Starts user process at highest priority.
    setpri(getpid(), 3);

    int timeslice, iterations, jobcount;
    char *job;
    timeslice = atoi(argv[1]);
    iterations = atoi(argv[2]);
    jobcount = atoi(argv[4]);
    job = argv[3];

    int child_pid;
    // List of all child pids for scheduling purposes.
    int child_pids[jobcount];
    char *job_args[1] = {job};
    for (int i = 0; i < jobcount; ++i) {
        // Start child process at lowest priority.
        child_pid = fork2(0);
        if (child_pid < 0) {
            printf(1, "Fork2 of given job failed.\n");
            exit();
        } else if (child_pid == 0) {
            // Child process
            exec(job, job_args);
            exit();
        } else {
            // Parent process
            // Saves child pid for scheduling use.
            child_pids[i] = child_pid;
        }
    }
    
    // Job scheduling
    for (int i = 0; i < iterations; ++i) {
        
        for(int j = 0; j < jobcount;j++ ){
            setpri(child_pids[j], 1);
            sleep(timeslice);
        }
    }

    // Kill processes
    for (int i = 0; i < jobcount; ++i) {
        kill(child_pids[i]);
    }
 

    struct pstat *pinfo = malloc(sizeof(*pinfo));
    int getpinfo_return = getpinfo(pinfo);
    if (getpinfo_return != 0) {
        printf(1, "getpinfo failed.\n");
    }

    for (int i = 0; i < jobcount; ++i) {
        wait();
    }

    // TODO: Call getpinfo and print out structure information.
    
    for (int i = 0; i < NPROC; ++i) {
        if (pinfo -> pid[i] == 0) {
            break;
        }
        printf(1, "-----------------------------------------------------------------\n");
        printf(1, "pid: %d, inuse: %d, priority: %d, state %s\n", pinfo->pid[i], pinfo->inuse[i], pinfo->priority[i], get_enum_string(pinfo -> state[i]));
        printf(1, "\n");
        printf(1, "ticks per priority state:\n");
        printf(1, "priority 0: %d, priority 1: %d, priority 2: %d, priority 3: %d\n", pinfo->ticks[i][0], pinfo->ticks[i][1], pinfo->ticks[i][2], pinfo->ticks[i][3]);
        printf(1, "\n");
        printf(1, "times moved to end of queue per priority state:\n");
        printf(1, "priority 0: %d, priority 1: %d, priority 2: %d, priority 3: %d\n", pinfo->qtail[i][0], pinfo->qtail[i][1], pinfo->qtail[i][2], pinfo->qtail[i][3]);
        printf(1, "\n");
    }
    exit();
}

// Returns string representation of enum value for printing.
const char *get_enum_string(enum procstate state) {
    switch (state) {
        case UNUSED: return "UNUSED";
        case EMBRYO: return "EMBRYO";
        case SLEEPING: return "SLEEPING";
        case RUNNABLE: return "RUNNABLE";
        case RUNNING: return "RUNNING";
        case ZOMBIE: return "ZOMBIE";
    }
    return "null";
}
