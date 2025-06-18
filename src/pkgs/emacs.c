#include <stdio.h>

#include "forge.h"

char *deps[] = {"libcurl", "gf", NULL};

char *getname(void) { return "emacs"; }
char *getver(void) { return "9.3.3"; }
char *getdesc(void) { return "A GNU Text Editor"; }
char **getdeps(void) { return deps; }
char *download(void) {return NULL;}
void build(void) {
        printf("Building emacs\n");
        printf("Done\n");
}
void install(void) {
        printf("Installing emacs\n");
        printf("Done\n");
}
void uninstall(void) {
        printf("Uninstalling emacs\n");
        printf("Done\n");
}

FORGE_GLOBAL pkg package = {
        .name = getname,
        .ver = getver,
        .desc = getdesc,
        .deps = getdeps,
        .download = download,
        .build = build,
        .install = install,
        .uninstall = uninstall,
};
