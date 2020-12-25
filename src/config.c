#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"

enum __OPTION {
    DAEMON_WORKER_MAX,
    REQUEST_QUEUE_MAX
};

static const char *optflags[] = {
    "DAEMON_WORKER_MAX",
    "REQUEST_QUEUE_MAX"
};

#define LINE_LENGTH_MAX 128
#define SEPARATOR '\t'
#define COMMENT '#'

int __load(enum __OPTION opt, const char *filename) {
    FILE *f = fopen(filename, "r");
    if (f == NULL) {
        return -1;
    }
    
    int ret = -1;

    char line[LINE_LENGTH_MAX] = { 0 };
    while (fgets(line, sizeof(line), f) != NULL) {
        if (*line == COMMENT || *line == '\n') {
            continue;
        }
        char flagbuf[LINE_LENGTH_MAX] = { 0 };
        int i = 0;
        while (line[i] != SEPARATOR) {
            flagbuf[i] = line[i];
            i++;
        }

        const char *flag = optflags[opt];
        if (strncmp(flag, flagbuf, strlen(flag)) == 0) {
            while (line[i] == SEPARATOR) {
                i++;
            }

            int j = 0;
            char retbuf[16];
            while (line[i] != '\0') {
                retbuf[j] = line[i];
                i++;
                j++;
            }

            ret = (int) strtol(retbuf, NULL, 10);
            break;
        }
    }

    fclose(f);
    return ret;
}

#define VALID_DAEMON_WORKER_MAX(x) (1 <= x && x <= 64)
#define VALID_REQUEST_QUEUE_MAX(x) (1 <= x && x <= 256)

int config_load(struct config *ptr, const char *filename) {
    int ret =  __load(DAEMON_WORKER_MAX, filename);
    if (ret == -1 || !VALID_DAEMON_WORKER_MAX(ret)) {
        return -1;
    }
    ptr->DAEMON_WORKER_MAX = (size_t) ret;

    ret = __load(REQUEST_QUEUE_MAX, filename);
    if (ret == -1 || !VALID_REQUEST_QUEUE_MAX(ret)) {
        return -1;
    }
    ptr->REQUEST_QUEUE_MAX = (size_t) ret;

    return 0;
}
