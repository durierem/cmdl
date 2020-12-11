#ifndef SQUEUE__H
#define SQUEUE__H

#include <stdbool.h>
#include "common.h"


#define SQ_LENGTH_MAX 16

typedef struct __squeue * SQueue;

extern SQueue sq_empty(void);
extern int sq_enqueue(SQueue sq, const struct request *rq);
extern int sq_dequeue(SQueue sq, struct request *rq);
extern size_t sq_length(const SQueue sq);
extern void sq_display(const SQueue sq);
extern void sq_dispose(SQueue *sqp);

#endif
