#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "inc/squeue.h"
#include "inc/config.h"

void die(const char *msg) {
    if (errno) {
        perror(msg);
    } else {
        fprintf(stderr, "%s\n", msg);
    }
    exit(EXIT_FAILURE);
}

int main(void) {
    SQueue shm_queue = sq_empty(sizeof(struct Request));
    if (shm_queue == NULL) {
        die("shm_queue");
    }

    return EXIT_SUCCESS;
}
