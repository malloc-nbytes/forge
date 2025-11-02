#include "msgs.h"

#include "forge/colors.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

static void
rest(void)
{
        usleep(500000/2);
}

void
info_builder(int newline, const char *first, ...)
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

        info(newline, result);

        free(result);
}

void
info(int newline, const char *msg)
{
        printf("%s" YELLOW BOLD "*" RESET " %s", newline ? "\n" : "", msg);
        rest();
}

void
bad(int newline, const char *msg)
{
        printf("%s" RED BOLD "* %s" RESET, newline ? "\n" : "", msg);
        rest();
}

void
good(int newline, const char *msg)
{
        printf("%s" GREEN BOLD "* %s" RESET, newline ? "\n" : "", msg);
        rest();
}
