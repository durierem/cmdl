#ifndef CONFIG__H
#define CONFIG__H

struct config {
    int DAEMON_WORKER_MAX;
    int REQUEST_QUEUE_MAX;
};

int config_load(struct config *ptr, const char *filename);

#endif
