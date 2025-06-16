#include "forge.h"

char *getname(void) { return "libcurl"; }
char *getver(void) { return "34.53.1"; }
char *getdesc(void) { return "An HTTP library"; }

FORGE_GLOBAL pkg package = {
        .name = getname,
        .ver = getver,
        .desc = getdesc,
        .deps = NULL,
};
