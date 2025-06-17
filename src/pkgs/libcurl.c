#include "forge.h"

char *deps[] = {"emacs", NULL};

char *getname(void) { return "libcurl"; }
char *getver(void) { return "34.53.1"; }
char *getdesc(void) { return "An HTTP library"; }
char **getdeps(void) { return deps; }
void build(void) {}
void install(void) {}
void uninstall(void) {}

FORGE_GLOBAL pkg package = {
        .name = getname,
        .ver = getver,
        .desc = getdesc,
        .deps = /*NULL*/getdeps,
        .build = build,
        .install = install,
        .uninstall = uninstall,
};
