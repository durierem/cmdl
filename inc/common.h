#ifndef COMMON__H
#define COMMON__H

#include <limits.h>

/* Nom associé au SHM pour stocker la file */
#define SHM_QUEUE "/cmdl_shm_queue"

/* Longueur maximale de l'argument aux fonction exec (possiblement défini) */
#ifndef ARG_MAX
#define ARG_MAX 2048
#endif

/* Longueur maximale pour les noms de chemins (possiblement défini) */
#ifndef PATH_MAX
#define PATH_MAX 2048
#endif

/* Signaux communiquant l'état des requêtes entre le daemon et les clients */
#define SIG_FAILURE SIGUSR1
#define SIG_SUCCESS SIGUSR2

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
