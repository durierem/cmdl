#include <fcntl.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "squeue.h"
#include "common.h"

/* file vide si tête de lecture et écirture au même endroit */
struct _squeue {
    int head;
    int tail;
    struct Request data[SQ_LENGTH_MAX];
    sem_t *slots_left;
};

SQueue sq_empty(void) {
    size_t shm_size = sizeof(struct _squeue); 

    int fd = shm_open(SHM_QUEUE, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        return NULL;
    }

    if (ftruncate(fd, (off_t) shm_size) == -1) {
        return NULL;
    }

    struct _squeue *sq = mmap(NULL, shm_size, PROT_READ | PROT_WRITE,
            MAP_SHARED, fd, 0);
    if (sq == MAP_FAILED) {
        return NULL;
    }

    sq->head = 0;
    sq->tail = 0;

    if (sem_init(sq->slots_left, 1, SQ_LENGTH_MAX) == -1) {
        return NULL;
    }

    return sq;
}

#define FUN_SUCCESS 0
#define FUN_FAILURE 42

int sq_enqueue(SQueue sq, struct Request *rq) {
    if (sq == NULL || rq == NULL) {
        return FUN_FAILURE;
    }

    if (sem_wait(sq->slots_left) == -1) {
        return FUN_FAILURE;
    }

    sq->data[sq->tail] = *rq;
    sq->tail = (sq->tail + 1) % SQ_LENGTH_MAX;

    return FUN_SUCCESS; 
}

void sq_dispose(SQueue *sqptr) {
    sem_destroy((*sqptr)->slots_left);
    shm_unlink(SHM_QUEUE);
}
