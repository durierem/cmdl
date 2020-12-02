#ifndef CONFIG__H
#define CONFIG__H

#include <limits.h>

#define SHM_OBJ_NAME "cmdl_shm_43264391123"

struct Request {
    char cmd[255];
    char pipe[255];
};

#endif
