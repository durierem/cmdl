#ifndef CONFIG__H
#define CONFIG__H

struct config {
    size_t DAEMON_WORKER_MAX;
    size_t REQUEST_QUEUE_MAX;
};

/**
 * Charge le fichier de configuration filename.
 *
 * @arg     ptr         Un pointeur vers une struct config.
 * @arg     filename    Le chemin du fichier de configuration.
 */
int config_load(struct config *ptr, const char *filename);

#endif
