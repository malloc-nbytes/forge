#include "forge.h"

//char *deps[] = {"libcurl", "gf", NULL};

char *getname(void) { return "emacs"; }
char *getver(void) { return "9.3.3"; }
char *getdesc(void) { return "A GNU Text Editor"; }
//char **getdeps(void) { return deps; }
void build(void) {}
void install(void) {}
void uninstall(void) {}

FORGE_GLOBAL pkg package = {
        .name = getname,
        .ver = getver,
        .desc = getdesc,
        .deps = /*getdeps*/NULL,
        .build = build,
        .install = install,
        .uninstall = uninstall,
};
