#ifndef SQUEUE__H
#define SQUEUE__H

#include <stdbool.h>

#define SQ_LENGTH_MAX 16

typedef struct __squeue * SQueue;

extern SQueue sq_empty(const char *shm_name, size_t size);
extern SQueue sq_open(const char *shm_name);
extern int sq_enqueue(SQueue sq, const void *obj);
extern int sq_dequeue(SQueue sq, void *buf);
extern size_t sq_length(const SQueue sq);
extern int sq_apply(SQueue sq, int (*fun)(void *));
extern void sq_dispose(SQueue *sqp);

#endif
