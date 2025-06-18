#include <stdio.h>
#include <unistd.h>

#include "colors.h"

void
good_major(const char *msg, int newline)
{
        printf(GREEN BOLD "*** %s" RESET, msg);
        if (newline) { putchar('\n'); }
        sleep(1);
}

void
good_minor(const char *msg, int newline)
{
        printf(GREEN "%s" RESET, msg);
        if (newline) { putchar('\n'); }
}

void
info_major(const char *msg, int newline)
{
        printf(YELLOW BOLD "*** %s" RESET, msg);
        if (newline) { putchar('\n'); }
        sleep(1);
}

void
info_minor(const char *msg, int newline)
{
        printf(YELLOW "%s" RESET, msg);
        if (newline) { putchar('\n'); }
}

void
bad_major(const char *msg, int newline)
{
        printf(RED BOLD "*** %s" RESET, msg);
        if (newline) { putchar('\n'); }
        sleep(1);
}

void
bad_minor(const char *msg, int newline)
{
        printf(RED "%s" RESET, msg);
        if (newline) { putchar('\n'); }
}
