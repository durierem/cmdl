#ifndef SQUEUE__H
#define SQUEUE__H

#include <stdbool.h>
#include "common.h"


#define SQ_LENGTH_MAX 16

typedef struct _squeue * SQueue;

extern SQueue sq_empty(void);
extern int sq_enqueue(SQueue sq, struct Request *rq);
extern void sq_dispose(SQueue *sqptr);

#endif
