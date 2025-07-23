#include <stdlib.h>
#include <stdio.h>
#include <regex.h>

#include "forge/utils.h"

int
forge_utils_regex(const char *pattern,
                  const char *s)
{
        regex_t regex;
        int reti;

        reti = regcomp(&regex, pattern, REG_ICASE);
        if (reti) {
                perror("regex");
                return 0;
        }

        reti = regexec(&regex, s, 0, NULL, 0);

        regfree(&regex);

        if (!reti) return 1;
        else return 0;
}
