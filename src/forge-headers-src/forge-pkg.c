#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "forge/pkg.h"
#include "forge/cmd.h"

int
forge_pkg_git_update(void)
{
        char buf[256] = {0};

        char *current_branch = cmdout("git rev-parse --abbrev-ref HEAD");
        cmd("git fetch origin");

        sprintf(buf, "git rev-parse %s", current_branch);
        char *local_commit = cmdout(buf);
        memset(buf, 0, sizeof(buf)/sizeof(*buf));

        sprintf(buf, "git rev-parse origin/%s", current_branch);
        char *remote_commit = cmdout(buf);
        memset(buf, 0, sizeof(buf)/sizeof(*buf));

        sprintf(buf, "git merge-base %s origin/%s", current_branch, current_branch);
        char *base_commit = cmdout(buf);
        memset(buf, 0, sizeof(buf)/sizeof(*buf));

        printf("current_branch: %s\n", current_branch);
        printf("local_commit: %s\n", local_commit);
        printf("remote_commit: %s\n", remote_commit);
        printf("base_commit: %s\n", base_commit);

        int res = -1;
        if (!strcmp(local_commit, remote_commit)) {
                res = 0;
        } else if (!strcmp(local_commit, base_commit)) {
                res = 1;
        }

        free(current_branch);
        free(local_commit);
        free(remote_commit);
        free(base_commit);

        return res;
}
