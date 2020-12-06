#ifndef LOGGER__H
#define LOGGER__H

#define LOG_MSG_MAX 510     /* 512 - (eol + newline) */

/**
 * Indique le niveau d'un log.
 *
 * L_INFO: log informatif
 * L_ERROR: log d'erreur
 */
enum LogLevel { L_INFO, L_ERROR };

/**
 * Définit le chemin vers le fichier de log à utiliser.
 * La validité de fichier filename est la responsabilité de l'utilisateur.
 */
extern void log_setfile(const char *filename);

/**
 * Écrit un log à la fin du fichier définit par log_setfile().
 *
 * Un log suit le format suivant:
 *      [YYYY-MM-DD HH:MM:SS] [<loglvl>] <msg>
 *
 * @param lvl Le niveau du log.
 * @param format Une chaîne formatée comme pour l'usage d'un printf().
 */
extern void log_puts(enum LogLevel lvl, const char *format, ...);

#endif
