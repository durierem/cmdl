#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include "squeue.h"
#include "config.h"

void mainloop(void);
void daemon(void);
void die(const char *msg);

int main(void) {
    switch (fork()) {
    case -1:
        die("fork");
        break;

    case 0:
        daemon();
        break;

    default:
        printf("Command Launcher daemon (successfully?) started.\n");
    }

    return EXIT_SUCCESS;
}

void mainloop() {
    while (1);
}

void daemon() {
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

    /* Exécute la boucle principale */
    mainloop();
    exit(EXIT_SUCCESS);
}

void die(const char *msg) {
    if (errno) {
        perror(msg);
    } else {
        fprintf(stderr, "%s: An error occured\n", msg);
    }
    exit(EXIT_FAILURE);
}
