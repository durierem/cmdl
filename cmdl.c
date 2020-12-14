#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "common.h"
#include "squeue.h"

int main(int argc, char *argv[]) {
    // gestion erreur
    
    char cmd[ARG_MAX];
    for (int i = 1; i < argc, i++) {
        sprintf(cmd, "%s ", argv[i]);
    }

    pid_t pid = getpid();

    char pipename[64];
    sprintf(pipename, "cmdl_pipe_%d", pid);

    struct request rq = { cmd, pipename, pid };
    
    SQueue sq = sq_open();

    if (sq_enqueue(sq, &rq) == -1) {
        fprintf(stderr, "mesesese");
        exit(EXIT_FAILURE);
    }

    int fd = mkfifo(pipename, O_RDONLY | O_CREAT | O_EXCL);
    if (fd == -1) {
        //erruer
        exit(EXIT_FAILURE);
    }

    if (unlink(pipename) == -1) {
        exit(EXIT_FAILURE);
    }

    // lecture du pipe + affichage sur le terminal

    if (close(pipename) == -1) {
        exit(EXIT_FAILURE);
    }

    return EXIT_SUCCESS;
}
