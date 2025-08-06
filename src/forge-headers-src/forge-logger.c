#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "forge/logger.h"

int
forge_logger_init(forge_logger    *logger,
                  const char      *fp,
                  forge_log_level  min_lvl)
{
        logger->fp = fopen(fp, "a");
        if (!logger->fp) return 0;

        logger->min_lvl = min_lvl;
        return 1;
}

void
forge_logger_log(forge_logger    *logger,
                 forge_log_level  lvl,
                 const char      *fmt,
                 ...)
{
        if (lvl < logger->min_lvl) return;

        time_t now = time(NULL);
        struct tm *tm_info = localtime(&now);
        char timestamp[20] = {0};
        strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", tm_info);

        fprintf(logger->fp, "[%s] %s: ", timestamp, forge_logger_level_to_cstr(lvl));

        va_list args;
        va_start(args, fmt);
        vfprintf(logger->fp, fmt, args);
        va_end(args);

        fprintf(logger->fp, "\n");
        fflush(logger->fp);
}

void
forge_logger_close(forge_logger *logger)
{
        if (logger && logger->fp) {
                fclose(logger->fp);
                logger->fp = NULL;
        }
}

const char *
forge_logger_level_to_cstr(forge_log_level lvl)
{
        switch (lvl) {
        case FORGE_LOG_LEVEL_DEBUG: return "DEBUG";
        case FORGE_LOG_LEVEL_INFO: return "INFO";
        case FORGE_LOG_LEVEL_WARN: return "WARN";
        case FORGE_LOG_LEVEL_ERR: return "ERR";
        case FORGE_LOG_LEVEL_FATAL: return "FATAL";
        default: return "UNKNOWN";
        }
        return NULL;
}
