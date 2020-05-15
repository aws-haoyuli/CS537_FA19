// Copyright 2019 Haoyu Li
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>

#define CMD_LEN 513
#define PROC_NUM 100
#define PARA_NUM 256
#define BURR_LEN 1024

// definition of Job
struct Job {
    int pid;
    int status;
    char cmdline[CMD_LEN];
};

// use an array to store jobs info
struct Job *jidToPid[PROC_NUM] = {NULL};
int jobId = 0;

// split cmd into an array, it will change cmd
int parseCmd(char *cmd, char **argv) {
    int i = 0;
    char *saveptr = NULL;
    char *pch = strtok_r(cmd, " \n", &saveptr);
    while (pch != NULL) {
        argv[i++] = pch;
        pch = strtok_r(NULL, " \n", &saveptr);
    }
    // the last should be NULL
    argv[i++] = pch;
    return i;
}

// return the position of > or -1; flag == 0 means there is &
int redirectFile(int flag, int argc, char *argv[]) {
    if (flag == 1 && argc > 4) {
        if (strcmp(argv[argc - 4], ">") == 0) {
            return argc - 4;
        }
    } else if (flag != 1 && argc > 3) {
        if (strcmp(argv[argc - 3], ">") == 0) {
            return argc - 3;
        }
    } else {
        // -1 means that there is no >
        return -1;
    }
    return -1;
}

// judge whether there is a "&" at the end, 1:background 0:foreground
int background(int argc, char *argv[]) {
    int flag = (argc >= 2) ? (strcmp(argv[argc - 2], "&") ? 0 : 1) : 0;
    if (flag == 1) {
        argv[argc - 2] = NULL;
    }
    return flag;
}

// put the job info into array, return the status for waitpid
int *addJob(int pid, int argc, char *argv[]) {
    // printf("pid = %d, cmdline = %s, jobId = %d\n", pid, *argv, jobId);
    // fflush(stdout);
    struct Job *newJob = (struct Job *) malloc(sizeof(struct Job));
    (newJob->cmdline)[0] = '\0';
    newJob->pid = pid;
    newJob->status = -1;
    for (int i = 0; i < argc - 1; i++) {
        if (argv[i] != NULL) {
            strncat((newJob->cmdline), argv[i], CMD_LEN);
            strncat((newJob->cmdline), " ", CMD_LEN);
        }
    }
    if (argv[argc - 1] != NULL) {
        strncat((newJob->cmdline), argv[argc - 1], CMD_LEN);
    } else {
        (newJob->cmdline)[strlen(newJob->cmdline) - 1] = '\0';
    }
    jidToPid[jobId++] = newJob;
    return &(newJob->status);
}

int isRunning(int jid) {
    waitpid(jidToPid[jid]->pid, &(jidToPid[jid]->status), WNOHANG);
    if (WIFEXITED(jidToPid[jid]->status)) {
        return 0;
    } else {
        char buffer[BURR_LEN];
        size_t length = snprintf(buffer, sizeof(buffer), "%d : %s\n",
                                 jid, jidToPid[jid]->cmdline);
        write(STDOUT_FILENO, buffer, length);
    }
//    write(STDERR_FILENO, "--------------\n", strlen("--------------\n"));
//    if (waitpid(jidToPid[jid]->pid, NULL, WNOHANG) == 0){
//        char buffer[BURR_LEN];
//        size_t length = snprintf(buffer, sizeof(buffer), "%d: %s\n",
//                                 jid, jidToPid[jid]->cmdline);
//        write(STDERR_FILENO, buffer, length);
//    }
    return 1;
}

int jobs() {
    for (int i = 0; i < jobId; i++) {
        isRunning(i);
    }
    return 0;
}

void waitJID(char *jidStr) {
    int jid = atoi(jidStr);
    if (jidToPid[jid] != NULL) {
        waitpid(jidToPid[jid]->pid, &(jidToPid[jid]->status), 0);
        char buffer[BURR_LEN];
        size_t length = snprintf(buffer, sizeof(buffer),
                                 "JID %d terminated\n", jid);
        write(STDOUT_FILENO, buffer, length);
    } else {
        char buffer[BURR_LEN];
        size_t length = snprintf(buffer, sizeof(buffer),
                                 "Invalid JID %d\n", jid);
        write(STDERR_FILENO, buffer, length);
    }
}

int runCmd(char *cmd) {
    char *argv[PARA_NUM];
    int argc = parseCmd(cmd, argv);

    // judge whether background, if yes, return 1
    int flag = background(argc, argv);

    // Additional flags or arguments appear along with built-in command
    // (e.g., jobs hello); do not treat as a built-in command
    // and create a new process with the specified arguments
    // The '&' character appears after a built-in command (e.g., exit &);
    // ignore the '&' character and execute normally.
    // If the '&' character appears in the middle of a line,
    // then the job should not be placed in the background;
    // instead, the '&' character is treated as one of the job arguments.
    if (argv[0] != NULL && (strcmp(argv[0], "exit") == 0) && argc - flag == 2) {
        return -1;
    }

    if (argv[0] != NULL && (strcmp(argv[0], "jobs") == 0) && argc - flag == 2) {
        jobs();
        return 0;
    }

    if (argv[0] != NULL && (strcmp(argv[0], "wait") == 0) && argc - flag == 3) {
        waitJID(argv[1]);
        return 0;
    }

    if (argc == 1) {
        return 0;
    }

    // judge whether redirect, if yes, return its position
    int pos = redirectFile(flag, argc, argv);
    // judge whether there is multiple redirection operators or multiple files
    // to the right of the redirection
    int tmp = (pos == -1) ? (argc - 1) : pos;
    if (flag == 1) {
        tmp--;
    }
    for (int i = 0; i < tmp; i++) {
        if (strcmp(argv[i], ">") == 0) {
            return 0;
        }
    }
    int fd_out = 0;
    // open file
    if (pos != -1) {
        fd_out = open(argv[pos + 1], O_CREAT | O_WRONLY,
                      S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    }

    // fork handle new process
    int child_pid = fork();
    if (child_pid == 0) {
        if (pos != -1) {
            // redirect STDIN/STDOUT to given file for this process
            // fd_out = open(argv[pos + 1], O_CREAT | O_RDWR);
            // truncate the file
            ftruncate(fd_out, 0);
            argv[pos] = NULL;
            if (fd_out > 0) {
                // redirect
                dup2(fd_out, STDOUT_FILENO);
                dup2(fd_out, STDERR_FILENO);
                // call shell command
                execvp(argv[0], argv);
                close(fd_out);
            }
        } else {
            // just execute it
            execvp(argv[0], argv);
        }
        // If execvp returns, it must have failed, print it
        char buffer[BURR_LEN];
        size_t length = snprintf(buffer, sizeof(buffer),
                                 "%s: Command not found\n", cmd);
        write(STDERR_FILENO, buffer, length);
        _exit(1);
    } else if (flag == 0) {
        // foreground mode, return child pid
        int *status = addJob(child_pid, argc, argv);
        waitpid(child_pid, status, 0);
    } else {
        // background mode, return child pid
        int *status = addJob(child_pid, argc, argv);
        waitpid(child_pid, status, WNOHANG);
    }

    // close the opened file
    if (pos != -1 && fd_out > 0) {
        close(fd_out);
    }

    // return pid
    return child_pid;
}

void destructor() {
    for (int i = 0; i < jobId; i++) {
        if (jidToPid[i] != NULL) {
            free(jidToPid[i]);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc == 2) {
        // batch mode
        FILE *fp = fopen(argv[1], "r");
        if (fp == NULL) {
            char buffer[BURR_LEN];
            size_t length = snprintf(buffer, sizeof(buffer),
                                     "Error: Cannot open file %s\n", argv[1]);
            write(STDERR_FILENO, buffer, length);
            destructor();
            return 1;
        }
        char line[CMD_LEN];
        while (fgets(line, CMD_LEN, fp)) {
            write(STDOUT_FILENO, line, strlen(line));
            if (runCmd(line) == -1) {
                destructor();
                return 0;
            }
        }
        fclose(fp);
    } else if (argc == 1) {
        // interactive mode
        char line[CMD_LEN];
        write(STDOUT_FILENO, "mysh> ", strlen("mysh> "));
        fgets(line, CMD_LEN, stdin);
//        write(STDOUT_FILENO, line, strlen(line));
        while (1) {
            if (runCmd(line) == -1) {
                destructor();
                return 0;
            }
            write(STDOUT_FILENO, "mysh> ", strlen("mysh> "));
            if (fgets(line, CMD_LEN, stdin) == NULL) {
                return 0;
            }
//            write(STDOUT_FILENO, line, strlen(line));
        }
    } else {
        // argument number error
        write(STDERR_FILENO, "Usage: mysh [batchFile]\n",
              strlen("Usage: mysh [batchFile]\n"));
        destructor();
        return 1;
    }

    return 0;
}


