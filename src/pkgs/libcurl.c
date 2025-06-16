#include "cpm.h"

char *getname(void) { return "libcurl"; }
char *getver(void) { return "34.53.1"; }
char *getdesc(void) { return "An HTTP library"; }

CPM_GLOBAL pkg package = {
        .name = getname,
        .ver = getver,
        .desc = getdesc,
        .deps = NULL,
        .installed = 1,
};
