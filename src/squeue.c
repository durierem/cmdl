#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "../inc/squeue.h"
#include "../inc/config.h"

struct squeue {
    size_t size;    /* Taille des éléments */
    size_t length;  /* Longueur courante de la file */
    size_t next;       /* Pointeur vers l'élément suivant de la file */
    char data[];
};

SQueue q_empty(size_t size) {
    size_t shm_size = sizeof(struct squeue) + SQ_LENGTH_MAX * size;

    int fd = shm_open(SHM_OBJ_NAME, O_RDWR | O_CREAT | O_EXCL,
            S_IRUSR | S_IWUSR);

    if (fd == -1) {
        return NULL;
    }

    /* Laisser le fichier pour que les clients puissent ouvrir la mémoire
     * partagée. Unlink à effectuer lors de la terminaison du démon seulement */

    if (shm_unlink(SHM_OBJ_NAME) == -1) {
        return NULL;
    }

    if (ftruncate(fd, shm_size) == -1) {
        return NULL;
    }

    volatile struct squeue *shm_queue = mmap(NULL, shm_size,
            PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (shm_queue == MAP_FAILED) {
        return NULL;
    }

    shm_queue->size = size;
    shm_queue->length = 0;
    shm_queue->next = shm_queue->data; /* (data + length * size) */
    return shm_queue;
}

const void *sq_enqueue(SQueue sq, const void *obj) {
    if (sq == NULL || sq->size != sizeof(*((struct squeue *) obj))) {
        return NULL;
    }

    memcpy(sq->next, obj, sizeof(*(struct squeue *) obj));
    const void *retval = sq->next;
    sq->length++;
    sq->next += sq->length * sq->size;
    return retval;
}

bool sq_isempty(const SQueue sq) {
    return sq->length == 0;
}

bool sq_isfull(const SQueue sq) {
    return sq->length == SQ_LENGTH_MAX;
}


