#ifndef LOG__H
#define LOG__H

/**
 * Indique le niveau d'un log.
 */
enum loglvl { INFO, ERROR };

/**
 * Écrit un log sur STDERR.
 *
 * Un log suit le format suivant:
 *      (YYYY-MM-DD HH:MM:SS) [<loglvl>] <msg>
 *
 * @param lvl Le niveau du log
 * @param format Une chaîne formatée comme pour l'usage d'un printf().
 */
extern void elog(enum loglvl lvl, const char *format, ...);

#endif
