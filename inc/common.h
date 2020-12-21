#ifndef CONFIG__H
#define CONFIG__H

#include <limits.h>

/* Nom associé au SHM pour stocker la file */
#define SHM_QUEUE "/cmdl_shm_queue"

#ifndef ARG_MAX
#define ARG_MAX 4096
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define THREAD_MAX  16

struct request {
    char cmd[ARG_MAX];
    char pipe[PATH_MAX];
    pid_t pid;
};

#endif
