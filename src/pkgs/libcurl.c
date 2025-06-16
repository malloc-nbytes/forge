#include "cpm.h"

char *deps[] = {NULL}; // Explicitly define empty dependencies

char *getname(void) { return "libcurl"; }
char *getver(void) { return "34.53.1"; }
char *getdesc(void) { return "An HTTP library"; }
char **getdeps(void) { return deps; }

CPM_GLOBAL pkg package = {
        .name = getname,
        .ver = getver,
        .desc = getdesc,
        .deps = getdeps,
        .installed = 0,
};
