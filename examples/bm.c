#include <forge/forge.h>

char *deps[] = {NULL}; // Must be NULL terminated

char *getname(void) { return "bm"; }
char *getver(void) { return "1.0.0"; }
char *getdesc(void) { return "Bookmark directories in the terminal"; }
char **getdeps(void) { return deps; }
char *download(void) {
        cmd("git clone https://www.github.com/malloc-nbytes/bm.git/");
        return "bm";
}
void build(void) {
        cmd("mkdir build");
        cd("build");
        cmd("cmake -S .. -B .");
        cmd("make");
}
void install(void) {
        cd("build");
        cmd("make install");
}
void uninstall(void) {
        cd("build");
        cmd("make uninstall");
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
        .update = forge_pkg_git_update,
};
