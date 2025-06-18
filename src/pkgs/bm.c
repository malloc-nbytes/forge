#include <stdio.h>

#include "forge.h"

char *getname(void) { return "bm"; }
char *getver(void) { return "1.0.2"; }
char *getdesc(void) { return "Keep track of your terminal bookmarks"; }
char *download(void) {
        cmd("git clone https://www.github.com/malloc-nbytes/bm.git/");
        return "bm";
}
void build(void) {
        printf("Building bm\n");
        cmd("mkdir build");
        cd("build");
        cmd("cmake -S .. -B .");
        cmd("make");
        printf("Done\n");
}
void install(void) {
        printf("Installing bm\n");
        cd("build");
        cmd("sudo make install");
        printf("Done\n");
}
void uninstall(void) {
        printf("Uninstalling bm\n");
        cd("build");
        cmd("sudo make uninstall");
        printf("Done\n");
}

FORGE_GLOBAL pkg package = {
        .name = getname,
        .ver = getver,
        .desc = getdesc,
        .deps = NULL,
        .download = download,
        .build = build,
        .install = install,
        .uninstall = uninstall,
};
