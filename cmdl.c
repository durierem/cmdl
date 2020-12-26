#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "common.h"
#include "squeue.h"

void sighandler(int sig);

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: cmdl '<command>'\n");
        exit(EXIT_FAILURE);
    }
    
    /* Affecte la gestion de SIG_FAILURE et SIG_SUCCESS a sighandler() */
    struct sigaction act;
    act.sa_handler = sighandler;
    act.sa_flags = 0;
    if (sigfillset(&act.sa_mask) == -1) {
        perror("sigfillset");
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIG_FAILURE, &act, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIG_SUCCESS, &act, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    /* Assure le passage de SIG_FAILURE */
    sigset_t set;
    if (sigemptyset(&set) == -1) {
        perror("sigemptyset");
        exit(EXIT_FAILURE);
    }
    if (sigaddset(&set, SIG_FAILURE) == -1) {
        perror("sigaddset");
        exit(EXIT_FAILURE);
    }
    if (sigprocmask(SIG_UNBLOCK, &set, NULL) == -1) {
        perror("sigprocmask");
        exit(EXIT_FAILURE);
    }

    /* Bloque temporairement le passage de SIG_SUCCESS */
    if (sigemptyset(&set) == -1) {
        perror("sigemptyset");
        exit(EXIT_FAILURE);
    }
    if (sigaddset(&set, SIG_SUCCESS) == -1) {
        perror("sigaddset");
        exit(EXIT_FAILURE);
    }
    if (sigprocmask(SIG_BLOCK, &set, NULL) == -1) {
        perror("sigprocmask");
        exit(EXIT_FAILURE);
    }

    /* Création de la requête */
    char cmd[ARG_MAX];
    snprintf(cmd, sizeof(cmd), argv[1]);

    pid_t pid = getpid();

    char pipe[PATH_MAX];
    snprintf(pipe, sizeof(pipe), "/tmp/cmdl_pipe_%d", pid);

    struct request rq;
    snprintf(rq.cmd, sizeof(rq.cmd), cmd);
    snprintf(rq.pipe, sizeof(rq.pipe), pipe);
    rq.pid = pid;
    
    /* Ouvre la file et enfile la requête */
    SQueue sq = sq_open(SHM_QUEUE);
    if (sq == NULL) {
        fprintf(stderr, "Error: failed to reach daemon.\n");
        exit(EXIT_FAILURE);
    }

    if (sq_enqueue(sq, &rq) == -1) {
        fprintf(stderr, "Error: failed to enqueue.\n");
        exit(EXIT_FAILURE);
    }

    /* Créé et ouvre le tube de communication */
    if (mkfifo(pipe, S_IRUSR | S_IWUSR) == -1) {
        perror("mkfifo");
        exit(EXIT_FAILURE);
    }

    int fd = open(pipe, O_RDONLY);
    if (fd == -1) {
        perror("open");
        exit(EXIT_FAILURE);
    }

    if (unlink(pipe) == -1) {
        perror("unlink");
        exit(EXIT_FAILURE);
    }

    /* Lecture depuis le tube et écriture sur STDOUT */
    struct stat st;
    if (fstat(fd, &st) == -1) {
        perror("fstat");
        exit(EXIT_FAILURE);
    }
    ssize_t blksize_in = st.st_blksize;

    if (fstat(STDOUT_FILENO, &st) == -1) {
        perror("fstat");
        exit(EXIT_FAILURE);
    }
    ssize_t blksize_out = st.st_blksize;

    char buf[blksize_in];
    ssize_t r;
    while ((r = read(fd, buf, (size_t) blksize_in)) > 0) {
        char *buf_out = buf;
        do {
            ssize_t btw = r > blksize_out ? blksize_out : r;
            if ((r = write(STDOUT_FILENO, buf_out, (size_t) btw)) == -1) {
                perror("write");
                exit(EXIT_FAILURE);
            }
            r -= btw;
            buf_out += btw;
        } while (r > 0);
    }

    /* Débloque le passage de SIG_SUCCESS */
    if (sigprocmask(SIG_UNBLOCK, &set, NULL) == -1) {
        perror("sigprocmask");
        exit(EXIT_FAILURE);
    }

    /* Place le processus en attente de SIG_SUCCESS */
    if (sigfillset(&set) == -1) {
        perror("sigfillset");
        exit(EXIT_FAILURE);
    }
    if (sigdelset(&set, SIG_FAILURE) == -1) {
        perror("sigdelset");
        exit(EXIT_FAILURE);
    }
    if (sigdelset(&set, SIG_SUCCESS) == -1) {
        perror("sigdelset");
        exit(EXIT_FAILURE);
    }
    sigsuspend(&set);
    if (errno != EINTR) {
        perror("sigsuspend");
        exit(EXIT_FAILURE);
    }

    return EXIT_SUCCESS;
}

void sighandler(int sig) {
    if (sig == SIG_FAILURE) {
        fprintf(stderr, "Error: request aborted.\n");
        exit(EXIT_FAILURE);
    }
    if (sig == SIG_SUCCESS) {
        exit(EXIT_SUCCESS);
    }
}
