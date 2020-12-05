/* TODO:
 * - Lier plus étroitement die() et elog()
 *   -> Déplacer die() vers le module de log ? Renommer le module ?
 *   -> Différencier les erreurs pre-daemon et le reste ?
 */

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
#include <unistd.h>

#include "config.h"
#include "log.h"
#include "squeue.h"

#define OPT_START "start"
#define OPT_STOP "stop"

/**
 * Libère les ressources allouées pour les descripteurs de fichiers,
 * les sémaphores, etc.
 */
void cleanup(void);

/**
 * Lance le processus de daemonisation.
 *
 * La daemonisation crée un processus fils détaché de tout terminal dont dont
 * l'entrée standard est redirigée vers /dev/null et les sorties standard et
 * d'erreur vers le fichier DAEMON_LOG_FILE.
 * À la fin du processus, le daemon ainsi créé contacte le processus parent
 * avec pipename.
 *
 * @param pipename Le nom du tube pour la communication avec le parent.
 */
void daemonise(const char *pipename);

/**
 * Termine le processus avec le code de retour EXIT_FAILURE.
 * 
 * La terminaison du programme est log dans DAEMON_LOG_FILE. Le log inclut
 * le message d'erreur fourni, ainsi que la valeur de errno et de strerror().
 * Avant de quitter, un appel à cleanup() est effectué.
 *
 * @param format Le message d'erreur à log, formatté comme pour printf().
 */
void die(const char *format, ...);

/**
 * Macro-fonction enveloppe pour la fonction die() : ajoute au message d'erreur
 * le nom du fichier et la ligne de l'erreur.
 */
#define died(msg, ...) die("(%s:%d) " msg, __FILE__, __LINE__, #__VA_ARGS__)

/**
 * Verouille le processus pour assurer son unicité en utilisant un mutex.
 *
 * @return Zéro s'il n'existe pas une autre instance du programme, -1 sinon.
 */
int lock(void);

/**
 * Démarre la boucle infinie principale du daemon.
 */
void mainloop(void);

int storepid(void);
int retrievepid(void);

/**
 * Affiche l'aide et quitte.
 */
void usage(void);

int main(int argc, char *argv[])
{
    /* Affiche l'aide si aucune option n'est spécifiée */
    if (argc < 2)
        usage();

    /* Gère la terminaison du daemon  déjà existant*/
    if (strcmp(argv[1], OPT_STOP) == 0) {
        if (lock() == 0) {
            fprintf(stderr, "[ERROR] No instance is running!\n");
            cleanup();
            exit(EXIT_FAILURE);
        }
        
        pid_t pid = retrievepid();
        if (pid == -1)
            die("retrievepid");

        if (kill(pid, SIGTERM) == -1)
            die("kill");

        exit(EXIT_SUCCESS);
    }

    if (strcmp(argv[1], OPT_START) != 0)
        usage();

    /* Vérifie la présence d'une autre instance du programme */
    if (lock() == -1) {
        fprintf(stderr, "[ERROR] Another instance is already running!\n");
        exit(EXIT_FAILURE);
    }

    /* Construit le nom d'un tube nommé à partir du PID du processus */
    pid_t pid = getpid();
    const char *pipeprefix = "/tmp/cmdld_pipe";
    char pipename[strlen(pipeprefix) + 8];
    sprintf(pipename, "%s%d", pipeprefix, pid);

    /* Créé le tube de communication avec le daemon */
    if (mkfifo(pipename, S_IRUSR | S_IWUSR) == -1)
        die("mkfifo");

    /* "fork off and die" */
    int fdpipe;
    switch (fork()) {
    case -1:
        die("fork");
        break;

    case 0:
        daemonise(pipename);
        break;

    default:
        /* open() bloquant jusqu'au contact par le daemon */
        fdpipe = open(pipename, O_RDONLY);
        if (fdpipe == -1)
            die("open");
        if (unlink(pipename) == -1)
            die("unlink");
        if (close(fdpipe) == -1)
            die("close");
    }

    return EXIT_SUCCESS;
}

void cleanup(void)
{
    sem_unlink(DAEMON_RUN_MUTEX);
    shm_unlink(DAEMON_SHM_PID);

    for (int i = 0; i < sysconf(_SC_OPEN_MAX); i++) {
        if (close(i) == -1 && errno == EBADF)
            break;
    }
}

void daemonise(const char *pipename)
{
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
    if (close(fd) == -1)
        die("close");
    
    /* Redirige STDOUT et STDERR vers le fichier de logs */
    fd = open(DAEMON_LOG_FILE, O_WRONLY | O_CREAT | O_APPEND,
            S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if (fd == -1)
        die("open");
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

    if (storepid() == -1)
        die("storepid");

    /* Exécute la boucle principale infinie */
    mainloop();

    exit(EXIT_SUCCESS);
}

void die(const char *format, ...)
{
    /* 
     * Si une erreur survient pendant le processus d'arrêt du programme et de
     * log, on veut garder le code d'erreur initial.
     */
    int errcode = errno; 

    va_list args;
    va_start(args, format);
    char msg[128]; // TODO: donner des constantes pour la taille des msg
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);

    elog(ERROR, "Daemon died with message: %s", msg);
    elog(ERROR, "\t=> errno (%d): %s", errcode, strerror(errcode));
    cleanup();
    exit(EXIT_FAILURE);
}

int lock(void)
{
    /* 
     * Note : l'appel à sem_unlink() est relégué à  fonction cleanup(). Cela
     * permet de garder le sémaphore en mémoire pour permettre à d'autres
     * processus de l'ouvrir.
     * Dans tous les cas, l'appel à sem_close() est effectué à la fin de la
     * fonction et permet de libérer les ressources allouées.
     */

    sem_t *sem = sem_open(DAEMON_RUN_MUTEX, O_RDWR | O_CREAT,
            S_IRUSR | S_IWUSR, 1);

    if (sem == SEM_FAILED)
        die("sem_open");
    
    int r = 0;
    if (sem_trywait(sem) == -1)
        r = -1;

    if (sem_close(sem) == -1)
        die("sem_close");

    return r;
}

void TERMhandler()
{
    elog(INFO, "Daemon terminated");
    cleanup();
    exit(EXIT_SUCCESS);
}

void mainloop(void)
{
    signal(SIGTERM, TERMhandler);
    elog(INFO, "Daemon started");

    /* Attente active -> pas bon pour l'usage CPU */
    while (1);
}

int storepid(void)
{
    size_t shm_size = sizeof(pid_t);

    int fd = shm_open(DAEMON_SHM_PID, O_RDWR | O_CREAT | O_EXCL,
            S_IRUSR | S_IWUSR);
    if (fd == -1)
        return -1;

    if (ftruncate(fd, (off_t) shm_size) == -1)
        return -1;

    pid_t *shm_pid = mmap(NULL, shm_size, PROT_WRITE, MAP_SHARED, fd, 0);
    if (shm_pid == MAP_FAILED)
        return -1;

    *shm_pid = getpid();

    return 0;
}

pid_t retrievepid(void)
{

    size_t shm_size = sizeof(pid_t);

    int fd = shm_open(DAEMON_SHM_PID, O_RDONLY, S_IRUSR);
    if (fd == -1)
        return -1;

    return *((pid_t *) mmap(NULL, shm_size, PROT_READ, MAP_SHARED, fd, 0));
}

void usage(void)
{
    printf("Usage: cmdld <start | stop>\n");
    exit(EXIT_FAILURE);
}

