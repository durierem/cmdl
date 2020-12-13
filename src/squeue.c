#include <assert.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "squeue.h"
#include "common.h"

struct __squeue {
    int head;
    int tail;
    size_t size;
    size_t length;
    sem_t mshm;
    sem_t mnfull;
    sem_t mnempty;
    char data[];
};

static void __sq_cleanup(struct __squeue *sq) {
    if (&sq->mshm != NULL) {
        sem_destroy(&sq->mshm);
    }

    if (&sq->mnfull != NULL) {
        sem_destroy(&sq->mnfull);
    }

    if (&sq->mnempty != NULL) {
        sem_destroy(&sq->mnempty);
    }

    shm_unlink(SHM_QUEUE);
}

SQueue sq_empty(size_t size) {
    size_t shm_size = sizeof(struct __squeue) + SQ_LENGTH_MAX * size;

    int fd = shm_open(SHM_QUEUE, O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
    if (fd == -1) {
        return NULL;

    }

    if (ftruncate(fd, (off_t) shm_size) == -1) {
        return NULL;
    }

    struct __squeue *sq = mmap(NULL, shm_size, PROT_READ | PROT_WRITE,
            MAP_SHARED, fd, 0);
    if (sq == MAP_FAILED) {
        return NULL;
    }

    close(fd);

    sq->head = 0;
    sq->tail = 0;
    sq->length = 0;
    sq->size = size;

    if (sem_init(&sq->mshm, 1, 1) == -1) {
        __sq_cleanup(sq);
        return NULL;
    }

    if (sem_init(&sq->mnfull, 1, SQ_LENGTH_MAX) == -1) {
        __sq_cleanup(sq);
        return NULL;
    }
    
    if (sem_init(&sq->mnempty, 1, 0) == -1) {
        __sq_cleanup(sq);
        return NULL;
    }

    return sq;
}

#define FUN_SUCCESS 0
#define FUN_FAILURE -1

int sq_enqueue(SQueue sq, const void *obj) {
    if (sq == NULL || obj == NULL) {
        return FUN_FAILURE;
    }

    if (sem_wait(&sq->mnfull) == -1) {
        return FUN_FAILURE;
    }

    if (sem_wait(&sq->mshm) == -1) {
        return FUN_FAILURE;
    }

    memcpy(sq->data + (size_t) sq->tail * sq->size, obj, sq->size);
    sq->tail = (sq->tail + 1) % SQ_LENGTH_MAX;
    sq->length++;
    
    if (sem_post(&sq->mshm) == -1) {
        return FUN_FAILURE;
    }

    if (sem_post(&sq->mnempty) == -1) {
        return FUN_FAILURE;
    }

    return FUN_SUCCESS; 
}

int sq_dequeue(SQueue sq, void *buf) {
    if (sq == NULL || buf == NULL) {
        return FUN_FAILURE;
    }

    if (sem_wait(&sq->mnempty) == -1) {
        return FUN_FAILURE;
    }
    
    if (sem_wait(&sq->mshm) == -1) {
        return FUN_FAILURE;
    }

    memcpy(buf, sq->data + (size_t) sq->head * sq->size, sq->size);
    sq->head = (sq->head + 1) % SQ_LENGTH_MAX;
    sq->length--;

    if (sem_post(&sq->mshm) == -1) {
        return FUN_FAILURE;
    }

    if (sem_post(&sq->mnfull) == -1) {
        return FUN_FAILURE;
    }

    return FUN_SUCCESS;
}

size_t sq_length(const SQueue sq) {
    return sq->length;
}

int sq_apply(SQueue sq, int (*fun)(void *)) {
    if (sq == NULL) {
        return FUN_FAILURE;
    }

    int i = sq->head;
    char *e = sq->data + (size_t) sq->head * sq->size;
    do {
        int ret = fun(e);
        if (ret != 0) {
            return ret;
        }
        i = (i + 1) % SQ_LENGTH_MAX;
        e = sq->data + (size_t) i * sq->size;
    } while (i != sq->tail);

    return FUN_SUCCESS;
}

void sq_dispose(SQueue *sqp) {
    __sq_cleanup(*((struct __squeue **) sqp));
    *sqp = NULL;
}
