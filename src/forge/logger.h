#ifndef FORGE_LOGGER_H_INCLUDED
#define FORGE_LOGGER_H_INCLUDED

#include <stdio.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

// The default location of the log file
#define FORGE_LOGGER_DEFAULT_FILEPATH "/tmp/forge-api-logger"

typedef enum {
        FORGE_LOG_LEVEL_DEBUG,
        FORGE_LOG_LEVEL_INFO,
        FORGE_LOG_LEVEL_WARN,
        FORGE_LOG_LEVEL_ERR,
        FORGE_LOG_LEVEL_FATAL,
} forge_log_level;

typedef struct {
        // Where are we logging to?
        FILE *fp;

        // The logger will not log any
        // entries that are lower than this.
        forge_log_level min_lvl;
} forge_logger;

/**
 * Parameter: logger   -> the uninitialized logger
 * Parameter: fp       -> the filepath to log to
 * Parameter: min_lvl  -> the minimum log level to report
 * Returns: 1 on success, 0 on failure
 * Description: Initialize the logger `logger`.
 */
int forge_logger_init(
        forge_logger *logger,
        const char *fp,
        forge_log_level min_lvl
);

/**
 * Parameter: logger -> the logger to use
 * Parameter: lvl    -> the level to report
 * Parameter: fmt    -> the format string
 * VARIADIC          -> other arguments
 * Description: Log the format string and arguments to
 *              the file in the logger.
 */
void forge_logger_log(
        forge_logger *logger,
        forge_log_level lvl,
        const char *fmt,
        ...
);

/**
 * Parameter: logger -> the logger to use
 * Description: Close the logger.
 */
void forge_logger_close(forge_logger *logger);

/**
 * Parameter: lvl -> the log level
 * Returns: a cstring equivalent
 * Description: Get a cstring version of the log level.
 */
const char *forge_logger_level_to_cstr(forge_log_level lvl);

#ifdef __cplusplus
}
#endif

#endif // FORGE_LOGGER_H_INCLUDED
