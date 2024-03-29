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
#define CFG_FILE "cmdld.conf"

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
 * Démarre le programme principal du daemon.
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
 * Compte le nombre d'arguments présents dans str.
 *
 * @arg str La chaîne à analyser.
 * @return Le nombre d'arguments dans str.
 */
size_t argcount(const char *str);

/**
 * Construit un tableau d'arguments à partir de str.
 *
 * Le tableau argv doit être de longueur au moins égale à argcount(str) + 1.
 * Il est utilisé pour y placer les pointeurs vers les chaînes dans buf. Le
 * dernier élément est un pointeur NULL.
 *
 * Le tampon buf doit être de taille au moins égale à la taille de la zone
 * mémoire pointée par str (strlen(str) + 1). Il est utilisé pour y placer les
 * chaînes que sont les arguments extraits de str.
 *
 * En cas de non respect des contraintes de taille sur argv et buf, le
 * comportement de strtoargs est indéfini.
 *
 * @arg     str     La chaîne à analyser.
 * @arg     argv    Le tableau d'arguments à remplir.
 * @arg     buf     Un tampon vide de même taille que str.
 */
void strtoargs(const char *str, char *argv[], char *buf);

/* --- MAIN ---------------------------------------------------------------- */

static SQueue g_queue;              /* La file en mémoire partagée */
static struct config g_config;      /* La configuration du daemon */
static struct worker *g_workers;    /* Liste des workers */

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

    /* Charge la configuration depuis CFG_FILE */
    if (config_load(&g_config, CFG_FILE) == -1) {
        fprintf(stderr, "Error: failed to load the configuration file.\n");
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

    /* Bloque tous les signaux pendant l'intitialisation */
    sigset_t set;
    if (sigfillset(&set) == -1) {
        die("sigfillset");
    }
    if (sigprocmask(SIG_SETMASK, &set, NULL) == -1) {
        die("sigprocmask");
    }

    /* Affecte SIGALRM et SIGTERM */
    struct sigaction act;
    act.sa_handler = sighandler;
    act.sa_flags = 0;
    if (sigfillset(&act.sa_mask) == -1) {
        die("sigfillset");
    }
    if (sigaction(SIGALRM, &act, NULL) == -1) {
        die("sigaction");
    }
    if (sigaction(SIGTERM, &act, NULL) == -1) {
        die("sigaction");
    }
    if (sigaction(SIG_SUCCESS, &act, NULL) == -1) {
        die("sigaction");
    }

    /* Laisse passer SIGALRM */
    if (sigdelset(&set, SIGALRM) == -1) {
        die("sigdelset");
    }
    if (sigdelset(&set, SIG_SUCCESS) == -1) {
        die("sigdelset");
    }
    if (sigprocmask(SIG_SETMASK, &set, NULL) == -1) {
        die("sigprocmask");
    }

    /* "fork off and die" */
    switch (fork()) {
    case -1:
        die("fork");
        break;

    case 0:
        daemonise(pipename);
        maind();
        exit(EXIT_SUCCESS);
        
    default:
        /* Envoie une alarme si le daemon n'a pas contacté dans les 5 sec */
        alarm(5);
        sigsuspend(&set);
        if (errno != EINTR) {
            die("sigsuspend");
        }
    }

    return EXIT_SUCCESS;
}

/* ------------------------------------------------------------------------- */

void cleanup(void) {
    /* Terminaison des threads */
    if (g_workers != NULL) {
        for (size_t i = 0; i < g_config.DAEMON_WORKER_MAX; i++) {
            struct worker wk = g_workers[i];
            pthread_cancel(wk.th);
            pthread_join(wk.th, NULL);
            kill(wk.rq.pid, SIG_FAILURE);
        }
    }

    /* Fermeture des descripteurs de fichiers */
    for (int i = 0; i < sysconf(_SC_OPEN_MAX); i++) {
        if (close(i) == -1 && errno == EBADF) {
            break;
        }
    }

    if (g_queue != NULL) {
        sq_dispose(&g_queue);
    }

    shm_unlink(DAEMON_SHM_PID);
    unlock();
}

void die(const char *format, ...) {
    /* Garde le code d'erreur initial si une autre erreur survient */
    int errcode = errno;
    const char *strerr = strerror(errcode);

    va_list args;
    va_start(args, format);
    char msg[256]; /* /!\ potentiel overflow  */
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);

    fprintf(stderr, "Error: daemon died '%s' (%s)", msg, strerr);
    syslog(LOG_ERR, "[maind] daemon died: '%s' (%s)", msg, strerr);

    cleanup();
    exit(EXIT_FAILURE);
}

void usage(void) {
    printf("Usage: cmdld <start | stop>\n");
    exit(EXIT_FAILURE);
}

/* ------------------------------------------------------------------------- */

void daemonise(const char *pipename) {
    printf("%s", pipename);
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

    /* Stocke le PID pour être contacté plus tard par un SIGTERM */
    if (storepid() == -1)
        die("storepid");

    /* Contacte le processus parent pour confirmer la daemonisation */
    if (kill(getppid(), SIG_SUCCESS) == -1)
       die("kill");
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

    /* Tableau des workers */
    struct worker wks[g_config.DAEMON_WORKER_MAX];
    g_workers = wks;

    /* Initialise les workers */
    for (size_t i = 0; i < g_config.DAEMON_WORKER_MAX; i++) {
        wks[i].id = (int) i;
        wks[i].avail = true;

        if (sem_init(&wks[i].mutex, 0, 0) == -1) {
            die("(sem_init) failed to initialise worker's mutex");
        }

        /* Assure la présence d'une valeur dans le champ pid pour que la
         * fonction cleanup() effectue des envois de signaux corrects dans le
         * cas où tout les workers n'auraient pas été utilisés; ceci afin de ne 
         * pas mécontenter valgrind. */
        wks[i].rq.pid = 0;

        int ret = pthread_create(&wks[i].th, NULL,
                (void *(*)(void *)) wkstart, &wks[i]);
        if (ret != 0) {
            die("(pthread_create) failed to create worker");
        }
    }

    syslog(LOG_INFO, "[maind] daemon started with %zu workers",
            g_config.DAEMON_WORKER_MAX);

    /* Boucle principale du daemon */
    struct request rq;
    while (sq_dequeue(g_queue, &rq) == 0) {
        syslog(LOG_DEBUG, "[maind] request dequeued { %s, %s, %d }",
                rq.cmd, rq.pipe, rq.pid);
        bool wk_found = false;
        for (size_t i = 0; i < g_config.DAEMON_WORKER_MAX; i++) {
            if (wks[i].avail) {
                wk_found = true;
                memcpy(&wks[i].rq, &rq, sizeof(struct request));
                if (sem_post(&wks[i].mutex) == -1) {
                    die("(sem_post) failed to unlock worker %d", wks[i].id);
                }
                syslog(LOG_DEBUG, "[maind] unlocked wk#%02d", wks[i].id);
                wks[i].avail = false;
                break;
            }
        }

        if (!wk_found) {
            if (kill(rq.pid, SIG_FAILURE) == -1) {
                die("(kill) failed to send %d to process %d", SIG_FAILURE,
                        rq.pid);
            }
        }
    }
}

void sighandler(int sig) {
    if (sig == SIGTERM) {
        cleanup();
        syslog(LOG_INFO, "[maind] daemon terminated");
        exit(EXIT_SUCCESS);
    }

    if (sig == SIGALRM) {
        die("failed to start");
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
        syslog(LOG_DEBUG, "[wk#%02d] locked (waiting)", wk->id);
        if (sem_wait(&wk->mutex) == -1) {
            syslog(LOG_ERR, "[wk#%02d] sem_wait: failed to lock worker's mutex",
                    wk->id);
        }

        syslog(LOG_DEBUG, "[wk#%02d] started running", wk->id);

        int fd;
        int status = EXIT_FAILURE;
        time_t tstart = time(NULL);

        char *argv[argcount(wk->rq.cmd) + 1];
        char buf[strlen(wk->rq.cmd) + 1];

        pid_t pid = fork();
        switch (pid) {
        case -1:
            syslog(LOG_ERR, "[wk#%02d] fork: failed to create child (%s)",
                    wk->id, strerror(errno));
            break;

        case 0:
            fd = open(wk->rq.pipe, O_WRONLY);
            if (fd == -1) {
                syslog(LOG_ERR, "[wk#%02d] open: failed to open '%s' (%s)",
                        wk->id, wk->rq.pipe, strerror(errno));
                exit(EXIT_FAILURE);
            }

            if (dup2(fd, STDOUT_FILENO) == -1) {
                syslog(LOG_ERR, "[wk#%02d] dup2: failed to redirect STDOUT (%s)",
                        wk->id, strerror(errno));
                exit(EXIT_FAILURE);
            }
            if (close(fd) == -1) {
                syslog(LOG_ERR, "[wk#%02d] close: failed to close '%s' (%s)",
                        wk->id, wk->rq.pipe, strerror(errno));
            }

            strtoargs(wk->rq.cmd, argv, buf);

            syslog(LOG_INFO, "[wk#%02d] started job '%s'", wk->id, wk->rq.cmd);
            execvp(argv[0], argv);
            syslog(LOG_ERR, "[wk#%02d] evecvp: failed to execute '%s' (%s)",
                wk->id, wk->rq.cmd, strerror(errno));
            exit(EXIT_FAILURE);
            break;

        default:
            waitpid(pid, &status, 0);
        }

        syslog(status == EXIT_SUCCESS ? LOG_INFO : LOG_ERR,
                "[wk#%02d] finished job '%s' (%lds) with status %d",
                wk->id, wk->rq.cmd, time(NULL) - tstart, status);

        int sig = (status == EXIT_SUCCESS ? SIG_SUCCESS : SIG_FAILURE);
        if (kill(wk->rq.pid, sig) == -1) {
            syslog(LOG_ERR, "[wk#%02d] kill: failed to send signal %d (%s)",
                    wk->id, sig, strerror(errno));
        } else {
            syslog(LOG_DEBUG, "[wk#%02d] sent signal %d to %d",
                    wk->id, sig, wk->rq.pid);
        }
        wk->avail = true;
    }
}

size_t argcount(const char *str) {
    if (*str == '\0') {
        return 0;
    }

    size_t n = 1;
    for (size_t i = 0; i < strlen(str); i++) {
        if (str[i] == ' ' && str[i - 1] == '-' && str[i - 2] == '-') {
            n++;
            break;
        }
        if (str[i] == ' ') {
            n++;
            while (str[i++] == ' ');
        }
    }

    return n;
}

void strtoargs(const char *str, char *argv[], char *buf) {
    memcpy(buf, str, strlen(str) + 1);

    size_t i = 0;
    size_t j = 0;

    argv[j++] = buf;

    while (j < argcount(str)) {
        if (str[i] == ' ') {
            buf[i] = 0;
            argv[j++] = buf + i + 1;
        }
        i++;
    }

    argv[j] = NULL;
}
