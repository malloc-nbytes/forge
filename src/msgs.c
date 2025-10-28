#include "msgs.h"

#include "forge/colors.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

void
info_builder(const char *first, ...)
{
        va_list args;
        size_t total_length = 0;

        // Get total length
        va_start(args, first);
        const char *str = first;
        while (str != NULL) {
                total_length += strlen(str);
                str = va_arg(args, const char*);
        }
        va_end(args);

        char *result = (char *)malloc(total_length + 1);

        size_t pos = 0;
        va_start(args, first);
        str = first;
        while (str != NULL) {
                strcpy(result + pos, str);
                pos += strlen(str);
                str = va_arg(args, const char*);
        }
        va_end(args);

        result[total_length] = '\0';

        info(result);

        free(result);
}

void
info(const char *msg)
{
        printf(YELLOW BOLD "*" RESET " %s", msg);
}

void
bad(const char *msg)
{
        printf(RED BOLD "* %s" RESET, msg);
}

void
good(const char *msg)
{
        printf(GREEN BOLD "* %s" RESET, msg);
}
