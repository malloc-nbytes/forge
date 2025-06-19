#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "forge/cmd.h"

int
cd(const char *fp)
{
        if (chdir(fp) != 0) {
                fprintf(stderr, "[forge] err: failed to change directory to `%s`: %s\n",
                        fp, strerror(errno));
                return 0;
        }
        return 1;
}

int
cd_silent(const char *fp)
{
        if (chdir(fp) != 0) {
                return 0;
        }
        return 1;
}

/* int */
/* cmd(const char *cmd) */
/* { */
/*         return system(cmd) != -1; */
/* } */

int
cmd(const char *cmd)
{
        printf("%s\n", cmd);

        FILE *fp = popen(cmd, "r");
        if (fp == NULL) {
                fprintf(stderr, "Failed to execute command '%s': %s\n", cmd, strerror(errno));
                return 0;
        }

        char buffer[1024];
        while (fgets(buffer, sizeof(buffer), fp) != NULL) {
                printf("%s", buffer);
        }

        // Get exit status
        int status = pclose(fp);
        if (status == -1) {
                fprintf(stderr, "Failed to close pipe for command '%s': %s\n", cmd, strerror(errno));
                return 0;
        }

        return WEXITSTATUS(status) == 0;
}
