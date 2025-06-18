#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include "forge.h"

#define FORGE_MAIN_DIR "/home/zdh/dev/forge/src";

static char *g_forge_cwd = FORGE_MAIN_DIR;

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
cmd(const char *cmd)
{
        return system(cmd) != -1;
}
