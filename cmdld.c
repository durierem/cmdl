#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <signal.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <syslog.h>
#include <unistd.h>

#include "common.h"
#include "squeue.h"

/* Les deux options possibles sur la ligne de commande */
#define OPT_START "start"
#define OPT_STOP "stop"
#define opt_test(opt) strcmp(opt, argv[1]) == 0 

/* Chemin vers le fichier de log du daemon */
#define LOG_FILE strcat(getenv("HOME"), "/.cmdld.log")

/* Nom associé au sémaphore qui assure l'unicité du daemon */
#define DAEMON_RUN_MUTEX "/cmdld_run_mutex"

/* Nom associé au SHM pour stocker le PID du daemon */
#define DAEMON_SHM_PID "/cmdld_shm_pid"

/**
 * Libère diverses ressources allouées pour le programme.
 *
 * Les ressources libérées sont celles qui ne doivent l'être qu'à la
 * terminaison du programme. Ceci inclut : descripteurs de fichiers,
 * sémaphores et segments de mémoire partagée.
 */
void cleanup(void);

/**
 * Lance le processus de daemonisation.
 *
 * La daemonisation crée un processus fils détaché de tout terminal dont
 * l'entrée standard et les sorties standard et d'erreur sont redirigées vers
 * /dev/null.
 * À la fin du processus, le daemon ainsi créé contacte le processus parent
 * avec pipename.
 *
 * @param pipename Le nom du tube pour la communication avec le parent.
 */
void daemonise(const char *pipename);

/**
 * Termine le processus avec le code de retour EXIT_FAILURE.
 * 
 * La terminaison du programme est log dans LOG_FILE. Le log inclut
 * le message d'erreur fourni, ainsi que la valeur de errno et le message
 * d'erreur associé. Avant de quitter, un appel à cleanup() est effectué.
 *
 * @param format Le message d'erreur à log, formatté comme pour printf().
 */
void die(const char *format, ...);

/**
 * Macro-fonction enveloppe pour la fonction die().
 * 
 * Ajoute au message d'erreur le nom du fichier et le numéro de la ligne à
 * laquelle l'erreur à été lancée.
 */
#define died(msg, ...) die("(%s:%d) " msg, __FILE__, __LINE__, #__VA_ARGS__)

/**
 * Tente de verouiller le processus pour assurer son unicité.
 *
 * Si le processus appellant est le premier à utiliser trylock(), la fonction
 * réussie, verouille un mutex, et renvoie 0. Tant que le processus appellant
 * n'a pas fait appel a unlock(), aucune autre instance du programme ne pourra
 * être lancée dans un autre processus. Un appel à unlock() sans avoir utilisé
 * trylock() au préalable échoue.
 *
 * @return 0 en cas de succès, -1 sinon.
 */
int trylock(void);
int unlock(void);

/**
 * Démarre la boucle infinie principale du daemon.
 */
void mainloop(void);

/**
 * Gestionnaire de signaux du daemon.
 */
void sighandler(int sig);

/**
 * Stocke/récupère le PID du processus dans DAEMON_SHM_PID.
 * 
 * @return Le PID stocké/récupéré en cas de succès, -1 sinon.
 */
pid_t storepid(void);
pid_t retrievepid(void);

/**
 * Affiche l'aide et quitte.
 */
void usage(void);

int main(int argc, char *argv[]) {
    /* Affiche l'aide si les options sont incorrectes */
    if (argc < 2 || !(opt_test(OPT_START) || opt_test(OPT_STOP))) {
        usage();
    }

    /* Gestion des options start/stop */
    bool isrunning = (trylock() == -1);
    if (opt_test(OPT_START) && isrunning) {
        fprintf(stderr, "Error: another instance is already running.\n");
        exit(EXIT_FAILURE);
    } else if (opt_test(OPT_STOP)) {
        if (!isrunning) {
            fprintf(stderr, "Error: no instance is running.\n");
            
            /* Puisqu'aucune autre instance n'est en cours, l'appel à trylock()
             * a réussi. Il faut donc unlock() pour permettre au futures
             * instances de pouvoir utiliser trylock() à nouveau. */
            unlock();
            exit(EXIT_FAILURE);
        }   

        pid_t pid = retrievepid();
        if (pid == -1) {
            fprintf(stderr, "Error: unable to retrieve the daemon's PID.\n");
            exit(EXIT_FAILURE);
        }

        if (kill(pid, SIGTERM) == -1) {
            fprintf(stderr, "Error: unable to send SIGTERM to the daemon.\n");
            exit(EXIT_FAILURE);
        }

        exit(EXIT_SUCCESS);
    }

    /* Ouvre la connexion au système de log */
    openlog("cmdld", LOG_PID, LOG_DAEMON);

    /* Construit le nom d'un tube nommé à partir du PID du processus */
    pid_t pid = getpid();
    const char *pipeprefix = "/tmp/cmdld_pipe";
    char pipename[strlen(pipeprefix) + 16];
    sprintf(pipename, "%s%d", pipeprefix, pid);

    /* Créé le tube de communication avec le daemon */
    if (mkfifo(pipename, S_IRUSR | S_IWUSR) == -1) {
        die("mkfifo");
    }

    /* "fork off and die" */
    int fdpipe;
    switch (fork()) {
    case -1:
        die("fork");
        break;

    case 0:
        daemonise(pipename);
        mainloop();
        exit(EXIT_SUCCESS);
        
    default:
        /* open() bloquant jusqu'au contact par le daemon
         *  -> possibilité de blocage infini
         *  -> utiliser un timer en parallèle ? */
        fdpipe = open(pipename, O_RDONLY);
        if (fdpipe == -1) {
            die("open");
        }

        if (unlink(pipename) == -1) {
            die("unlink");
        }

        if (close(fdpipe) == -1) {
            die("close");
        }
    }

    return EXIT_SUCCESS;
}

void cleanup(void) {
    unlock();
    shm_unlink(DAEMON_SHM_PID);
    for (int i = 0; i < sysconf(_SC_OPEN_MAX); i++) {
        /* Stoppe sur le premier descripteur non valide */
        if (close(i) == -1 && errno == EBADF) {
            break;
        }
    }
}

void daemonise(const char *pipename) {
    /* Détache le daemon de la session actuelle */
    if (setsid() == -1)
        die("setsid");

    /* Libère le répertoire de travail actuel */
    if (chdir("/") == -1)
        die("chdir");

    /* Permet aux appels systèmes d'utiliser leur propre umask */
    umask(0);

    /* Redirige STDIN vers /dev/null */
    int fd = open("/dev/null", O_RDWR);
    if (fd == -1)
        die("open");
    if (dup2(fd, STDIN_FILENO) == -1)
        die("dup2");
    if (dup2(fd, STDOUT_FILENO) == -1)
        die("dup2");
    if (dup2(fd, STDERR_FILENO) == -1)
        die("dup2");
    if (close(fd) == -1)
        die("close");
    
    /* Contacte le processus parent pour confirmer la daemonisation */
    fd = open(pipename, O_WRONLY);
    if (fd == -1)
        die("open");
    if (close(fd) == -1)
        die("close");

    /* Stocke le PID pour être contacté plus tard par un SITERM */
    if (storepid() == -1)
        die("storepid");

    /* Gestion des signaux : bloque tout sauf SIGTERM */
    struct sigaction act;
    act.sa_handler = sighandler;
    act.sa_flags = 0;
    if (sigfillset(&act.sa_mask) == -1)
        die("sigfillset");
    if (sigprocmask(SIG_SETMASK, &act.sa_mask, NULL) == -1)
        die("sigprocmask");
    if (sigaction(SIGTERM, &act, NULL) == -1)
        die("sigaction");
}

void die(const char *format, ...) {
    /* Garde le code d'erreur initial si une autre erreur survient */
    int errcode = errno; 

    va_list args;
    va_start(args, format);
    char msg[256]; /* /!\ potentiel overflow  */
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);

    syslog(LOG_ERR, "Daemon died: '%s', errno=%d '%s'",
            msg, errcode, strerror(errcode));

    cleanup();
    exit(EXIT_FAILURE);
}

/* Note : l'appel à sem_unlink() est relégué à la fonction unlock().
 * Cela permet de garder le sémaphore en mémoire pour permettre à
 * d'autres processus de l'ouvrir.
 * Dans tous les cas, l'appel à sem_close() est effectué à la fin de la
 * fonction et permet de libérer les ressources allouées. */
int trylock(void) {
    sem_t *sem = sem_open(DAEMON_RUN_MUTEX, O_RDWR | O_CREAT,
            S_IRUSR | S_IWUSR, 1);

    if (sem == SEM_FAILED) {
        return -1;
    }

    int r = 0;
    if (sem_trywait(sem) == -1) {
        r = -1;
    }

    if (sem_close(sem) == -1) {
        return -1;
    }

    return r;
}

int unlock(void) {
    return sem_unlink(DAEMON_RUN_MUTEX);
}

void mainloop(void) {
    syslog(LOG_INFO, "Daemon started");

    /* Attente active -> pas bon pour l'usage CPU */
    while (1);
}

void sighandler(int sig) {
    if (sig == SIGTERM) {
        syslog(LOG_INFO, "Daemon terminated");
        cleanup();
        exit(EXIT_SUCCESS);
    }
}

pid_t storepid(void) {
    size_t shm_size = sizeof(pid_t);

    int fd = shm_open(DAEMON_SHM_PID, O_RDWR | O_CREAT | O_EXCL,
            S_IRUSR | S_IWUSR);
    if (fd == -1) {
        return -1;
    }

    if (ftruncate(fd, (off_t) shm_size) == -1) {
        return -1;
    }

    pid_t *shm_pid = mmap(NULL, shm_size, PROT_WRITE, MAP_SHARED, fd, 0);
    if (shm_pid == MAP_FAILED) {
        return -1;
    }

    *shm_pid = getpid();

    return *shm_pid;
}

pid_t retrievepid(void) {
    size_t shm_size = sizeof(pid_t);

    int fd = shm_open(DAEMON_SHM_PID, O_RDONLY, S_IRUSR);
    if (fd == -1) {
        return -1;
    }

    return *((pid_t *) mmap(NULL, shm_size, PROT_READ, MAP_SHARED, fd, 0));
}

void usage(void) {
    printf("Usage: cmdld <start | stop>\n");
    exit(EXIT_FAILURE);
}

