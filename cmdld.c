#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>

#include "common.h"
#include "config.h"
#include "squeue.h"

/* --- DIVERS -------------------------------------------------------------- */

/* Les deux options possibles sur la ligne de commande */
#define OPT_START "start"
#define OPT_STOP "stop"
#define opt_test(opt) strcmp(opt, argv[1]) == 0

/* Le chemin vers le fichier de configuration du daemon */
#define g_config_FILE "cmdld.conf"

/**
 * Libère diverses ressources allouées pour le programme.
 *
 * Les ressources libérées sont celles qui ne doivent l'être qu'à la
 * terminaison du programme. Ceci inclut : descripteurs de fichiers,
 * sémaphores et segments de mémoire partagée.
 */
void cleanup(void);

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
 * Affiche l'aide et quitte.
 */
void usage(void);

/* --- DAEMON -------------------------------------------------------------- */

/* Nom associé au sémaphore qui assure l'unicité du daemon */
#define DAEMON_RUN_MUTEX "/cmdld_run_mutex"

/* Nom associé au SHM pour stocker le PID du daemon */
#define DAEMON_SHM_PID "/cmdld_shm_pid"

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
void maind(void);

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

/* --- WORKERS ------------------------------------------------------------- */

/**
 * Structure contenant les informations d'un worker.
 *
 * @field   id      Un numéro d'identification.
 * @field   th      Le thread associé.
 * @field   mutex   Sémaphore de mise en attente.
 * @field   avail   Indique la disponibilité du worker.
 * @field   rq      La requête qu'exécute le worker.
 */
struct worker {
    int id;
    pthread_t th;
    sem_t mutex;
    bool avail;
    struct request rq;
};

/**
 * Fonction de démarrage des workers.
 *
 * La fonction lance une boucle infinie et le thread associé au worker se met
 * en attente d'une requête. La requête est effectuée dans un processus fils,
 * et une entrée est ajoutée aux logs du daemon.
 *
 * @arg wk Un pointeur vers un worker.
 */
void *wkstart(struct worker *wk);

/**
 * Transforme une chaîne de caractère en un tableau de mots la constituant.
 */
char **parse_arg(const char *str);

/* --- MAIN ---------------------------------------------------------------- */

static SQueue g_queue;          /* La file en mémoire partagée */
static struct config g_config;  /* La configuration du daemon */

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

    if (config_load(&g_config, g_config_FILE) == -1) {
        fprintf(stderr, "Error: failed to load configuration file.\n");
        exit(EXIT_FAILURE);
    }

    /* Ouvre la connexion au système de log */
    openlog("cmdld", LOG_PID, LOG_DAEMON);

    /* Construit le nom d'un tube nommé à partir du PID du processus */
    pid_t pid = getpid();
    char pipename[PATH_MAX];
    sprintf(pipename, "/tmp/cmdld_pipe.%d", pid);

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
        maind();
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

/* ------------------------------------------------------------------------- */

void cleanup(void) {
    sq_dispose(&g_queue);
    unlock();
    shm_unlink(DAEMON_SHM_PID);
    for (int i = 0; i < sysconf(_SC_OPEN_MAX); i++) {
        /* Stoppe sur le premier descripteur non valide */
        if (close(i) == -1 && errno == EBADF) {
            break;
        }
    }
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

void usage(void) {
    printf("Usage: cmdld <start | stop>\n");
    exit(EXIT_FAILURE);
}

/* ------------------------------------------------------------------------- */

void daemonise(const char *pipename) {
    /* Détache le daemon de la session actuelle */
    if (setsid() == -1)
        die("setsid");

    /* Libère le répertoire de travail actuel */
    if (chdir("/") == -1)
        die("chdir");

    /* Permet aux appels systèmes d'utiliser leur propre umask */
    umask(0);

    /* Redirige STDIN, STDOUT et STDERR vers /dev/null */
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

    /* Stocke le PID pour être contacté plus tard par un SIGTERM */
    if (storepid() == -1)
        die("storepid");
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

void maind(void) {
    /* Affecte sighandler() à la gestion de SIGTERM */
    struct sigaction act;
    act.sa_handler = sighandler;
    act.sa_flags = 0;
    if (sigfillset(&act.sa_mask) == -1) {
        die("sigfillset");
    }
    if (sigaction(SIGTERM, &act, NULL) == -1) {
        die("sigaction");
    }
    
    /* Masque tous les signaux sauf SIGTERM */
    sigset_t masked;
    if (sigfillset(&masked) == -1) {
        die("sigfillset");
    }
    if (sigdelset(&masked, SIGTERM) == -1) {
        die("sigdelset");
    }
    if (sigprocmask(SIG_SETMASK, &masked, NULL) == -1) {
       die("sigprocmask");
    }
    
    /* Initialise la file de requêtes */
    g_queue = sq_empty(SHM_QUEUE, sizeof(struct request),
                       (size_t) g_config.REQUEST_QUEUE_MAX);
    if (g_queue == NULL) {
        die("sq_empty");
    }

    struct worker wks[g_config.DAEMON_WORKER_MAX];

    for (int i = 0; i < g_config.DAEMON_WORKER_MAX; i++) {
        wks[i].id = i;
        int ret = pthread_create(&wks[i].th, NULL, (void *(*)(void *)) wkstart,  
                &wks[i]);
        if (ret != 0) {
            die("(pthread_create) failed to create worker");
        }
        wks[i].avail = true;
        if (sem_init(&wks[i].mutex, 0, 0) == -1) {
            die("(sem_init) failed to initialise worker's mutex");
        }
    }

    syslog(LOG_INFO, "Daemon started with %d workers", g_config.DAEMON_WORKER_MAX);

    struct request rq;
    while (sq_dequeue(g_queue, &rq) == 0) {
        bool wk_found = false;
        for (int i = 0; i < g_config.DAEMON_WORKER_MAX; i++) {
            if (wks[i].avail) {
                wk_found = true;
                memcpy(&wks[i].rq, &rq, sizeof(struct request));
                if (sem_post(&wks[i].mutex) == -1) {
                    die("(sem_post) failed to unlock worker %d", i);
                }
                break;
            }
        }

        if (!wk_found) {
            if (kill(rq.pid, SIGUSR1) == -1) {
                die("(kill) failed to send SIGUSR1 to process %d", rq.pid);
            }
        }
    }
}

void sighandler(int sig) {
    if (sig == SIGTERM) {
        cleanup();
        syslog(LOG_INFO, "Daemon terminated");
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

/* ------------------------------------------------------------------------- */

void *wkstart(struct worker *wk) {
    while (1) {
        if (sem_wait(&wk->mutex) == -1) {
            die("Wk %d: (sem_wait) failed to lock worker's mutex", wk->id);
        }

        wk->avail = false;

        int fd;
        int status;
        time_t tstart = time(NULL);

        switch (fork()) {
        case -1:
            die("fork");
            break;

        case 0:
            fd = open(wk->rq.pipe, O_WRONLY);
            if (fd == -1) {
                syslog(LOG_ERR, "Wk %d: (open) failed to open '%s'", wk->id,
                        wk->rq.pipe);
                exit(EXIT_FAILURE);
            }
            if (dup2(fd, STDOUT_FILENO) == -1) {
                syslog(LOG_ERR, "Wk %d: (dup2) failed to redirect STDOUT",
                        wk->id);
                exit(EXIT_FAILURE);
            }
            if (close(fd) == -1) {
                syslog(LOG_ERR, "Wk %d: (close) failed to close '%s'", wk->id,
                        wk->rq.pipe);
            }

            char **cmd = parse_arg(wk->rq.cmd);
            int i = 0;
            char *str = cmd[i];
            while (str != NULL) {
                syslog(LOG_DEBUG, "%s", str);
                i++;
                str = cmd[i];
            }

            syslog(LOG_INFO, "Wk %d: starts job '%s'", wk->id, wk->rq.cmd);
            execvp(cmd[0], cmd);
            syslog(LOG_ERR, "Wk %d: (evecvp) failed to execute '%s'", wk->id,
                    wk->rq.cmd);
            exit(EXIT_FAILURE);
            break;

        default:
            wait(&status);
            syslog(status == EXIT_SUCCESS ? LOG_INFO : LOG_ERR,
                    "Wk %d: finished job (%lds) with status %d",
                    wk->id, time(NULL) - tstart, status);
            if (status != EXIT_SUCCESS) {
                if (kill(wk->rq.pid, SIGUSR2) == -1) {
                    syslog(LOG_ERR, "Wk %d: (kill) failed to send SIGUSR2",
                            wk->id);
                }
            }
            wk->avail = true;
        }
    }
}



char **parse_arg(const char *arg) {
    size_t nb_words = 1;
    for (size_t i = 0; i < strlen(arg); i++) {
        if (arg[i] == ' ') {
            nb_words++;
        }
    }
    
    // char *result[nb_words * sizeof(char *) + 1];
    char **result = malloc(nb_words * sizeof(char *) + 1);
    if (result == NULL) {
        perror("malloc()");
        exit(EXIT_FAILURE);
    }

    // char argcp[strlen(arg)];
    char *argcp = malloc(strlen(arg));
    if (argcp == NULL) {
        perror("malloc()");
        exit(EXIT_FAILURE);
    }
    strcpy(argcp, arg);

    char *token = strtok(argcp, " ");
    int i = 0;
    while (token != NULL) {
        result[i] = token;
        token = strtok(NULL, " ");
        i++;
    }
    result[nb_words + 1] = NULL;

    return result;
}
