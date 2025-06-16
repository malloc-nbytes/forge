#include <stdio.h>
#include <stdlib.h>

#include "flags.h"

void
usage(void)
{
        printf("Usage: forge [options...]\n");
        printf("Options:\n");
        printf("    -%c, --%s    display this message\n", FLAG_1HY_HELP, FLAG_2HY_HELP);
        printf("        --%s    list installed packages\n", FLAG_2HY_LIST);
        exit(0);
}
