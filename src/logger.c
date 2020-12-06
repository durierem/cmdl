#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "config.h"
#include "logger.h"

#define DEFAULT_FILE "/tmp/default_log"

#define INFO_PREFIX   "[INFO] "
#define ERROR_PREFIX  "[ERROR] "
#define TIME_FORMAT   "[%F %T] "

#define LVPREFIX_SIZE 6
#define TIME_PREFIX_SIZE 22 

static const char *logfile = DEFAULT_FILE;
static char msg[LOG_MSG_MAX + 1];

void log_setfile(const char *filename)
{
    logfile = filename;
}

void log_puts(enum LogLevel lvl, const char *format, ...)
{
    char timeprefix[TIME_PREFIX_SIZE + 1];

    time_t t = time(NULL);
    struct tm *lt = localtime(&t);
    strftime(timeprefix, sizeof(timeprefix), TIME_FORMAT, lt);
    
    const char *lvlprefix;
    switch (lvl) {
        case L_INFO:    lvlprefix = INFO_PREFIX; break;
        case L_ERROR:   lvlprefix = ERROR_PREFIX; break;
        default:        lvlprefix = INFO_PREFIX;
    }

    size_t prefix_size = strlen(timeprefix) + strlen(lvlprefix);

    va_list args;
    va_start(args, format);

    snprintf(msg, prefix_size + 1, "%s%s", timeprefix, lvlprefix);
    vsnprintf(msg + prefix_size, sizeof(msg) - prefix_size, format, args);

    va_end(args);

    FILE *f = fopen(logfile, "a");
    if (f != NULL) {
        fprintf(f, "%s\n", msg);
        fclose(f);
    }
}

