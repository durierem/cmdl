#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "config.h"
#include "log.h"

#define LOG_IPREFIX "[INF] "
#define LOG_EPREFIX "[ERR] "
#define LOG_TFORMAT "(%F %T) "
#define LOG_MSG_MAX 255

static char msg[LOG_MSG_MAX];

void elog(enum loglvl lvl, const char *format, ...)
{
    va_list args;
    va_start(args, format);

    char timeprefix[23];    // Taille de la date + 1
    char lvlprefix[7];      // Taille du préfixe + 1

    time_t t = time(NULL);
    struct tm *lt = localtime(&t);
    strftime(timeprefix, 23, LOG_TFORMAT, lt);

    switch (lvl) {
    case INFO:
        snprintf(lvlprefix, sizeof(lvlprefix), "%s", LOG_IPREFIX);
        break;

    case ERROR:
        snprintf(lvlprefix, sizeof(lvlprefix), "%s", LOG_EPREFIX);
        break;
    }

    size_t prefix_size = sizeof(timeprefix) + sizeof(lvlprefix);
    snprintf(msg, prefix_size, "%s%s", timeprefix, lvlprefix);
    vsnprintf(msg + 28 , sizeof(msg) - 28, format, args);
    fprintf(stderr, "%s\n", msg);

    va_end(args);
}

