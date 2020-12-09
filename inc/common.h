#ifndef CONFIG__H
#define CONFIG__H

#include <limits.h>
#include <stdlib.h>
#include <string.h>

/* Nom associé au SHM pour stocker la file */
#define SHM_QUEUE "/cmdl_shm_queue"

#ifndef ARG_MAX
#define ARG_MAX 4096
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define THREAD_MAX  16
#define CLIENT_MAX  32

struct Request {
    char cmd[ARG_MAX];
    char pipe[PATH_MAX];
};

#endif
