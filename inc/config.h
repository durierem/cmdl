#ifndef CONFIG__H
#define CONFIG__H

struct config {
    size_t DAEMON_WORKER_MAX;
    size_t REQUEST_QUEUE_MAX;
};

int config_load(struct config *ptr, const char *filename);

#endif
