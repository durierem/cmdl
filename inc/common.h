#ifndef COMMON__H
#define COMMON__H

#include <limits.h>

/* Nom associé au SHM pour stocker la file */
#define SHM_QUEUE "/cmdl_shm_queue"

#ifndef ARG_MAX
#define ARG_MAX 2048
#endif

#ifndef PATH_MAX
#define PATH_MAX 2048
#endif

/**
 * Structure représentant une requête.
 *
 * @field   cmd     La commande à exécuter.
 * @field   pipe    Le nom du tube vers lequel rediriger la sortie.
 * @field   pid     Le PID du client appellant.
 */
struct request {
    char cmd[ARG_MAX];
    char pipe[PATH_MAX];
    pid_t pid;
};

#endif
