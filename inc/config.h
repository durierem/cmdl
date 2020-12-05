#ifndef CONFIG__H
#define CONFIG__H

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define SHM_QUEUE "/cmdl_shm_queue"

#define DAEMON_LOG_FILE strcat(getenv("HOME"), "/.cmdld.log")
#define DAEMON_RUN_MUTEX "/cmdld_run_mutex"
#define DAEMON_SHM_PID "/cmdld_shm_pid"

#ifndef ARG_MAX
#define ARG_MAX 4096
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define THREAD_MAX  16
#define CLIENT_MAX  32

struct Request {
    char cmd[ARG_MAX];
    char pipe[PATH_MAX];
};

#endif
