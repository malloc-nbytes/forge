#include "forge.h"

char *deps[] = {"libcurl", NULL};

char *getname(void) { return "gf"; }
char *getver(void) { return "1.0.0"; }
char *getdesc(void) { return "A GDB frontend debugger"; }
char **getdeps(void) { return deps; }

FORGE_GLOBAL pkg package = {
        .name = getname,
        .ver = getver,
        .desc = getdesc,
        .deps = getdeps,
};
