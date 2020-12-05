#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "squeue.h"
#include "config.h"

struct squeue {
    size_t size;    /* Taille des éléments */
    char *start;    /* Début de la file */
    char *end;      /* Fin de la file */
    char data[];    /* Zone mémoire dédiée à la file */
};

SQueue sq_empty(size_t size) {
    size_t shm_size = sizeof(struct squeue) + SQ_LENGTH_MAX * size;

    int fd = shm_open(SHM_QUEUE, O_RDWR | O_CREAT | O_EXCL,
            S_IRUSR | S_IWUSR);

    if (fd == -1) {
        return NULL;
    }

    /* /!\ À supprimer
     * Il faut laisser le fichier pour que les clients puissent ouvrir la mémoire
     * partagée. Unlink à effectuer lors de la terminaison du démon seulement */
    if (shm_unlink(SHM_QUEUE) == -1) {
        return NULL;
    }

    if (ftruncate(fd, (off_t) shm_size) == -1) {
        return NULL;
    }

    struct squeue *shm_queue = mmap(NULL, shm_size,
            PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if (shm_queue == MAP_FAILED) {
        return NULL;
    }

    shm_queue->size = size;
    shm_queue->start = shm_queue->data;
    shm_queue->end = shm_queue->data;
    return shm_queue;
}

#define FUN_SUCCESS 0
#define FUN_FAILURE 42

int sq_enqueue(SQueue sq, const void *obj) {
    if (sq == NULL || obj == NULL) {
        return FUN_FAILURE;
    }

    /* DÉBUT SECTION CRITIQUE */
    memcpy(sq->end, obj, sq->size);
    sq->end += sq->size;
    /* FIN SECTION CRITIQUE */

    return FUN_SUCCESS; 
}
