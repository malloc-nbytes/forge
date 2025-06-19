#include <forge/forge.h>

char *deps[] = {NULL}; // Must be NULL terminated

char *getname(void) { return "far"; }
char *getver(void) { return "1.0.0"; }
char *getdesc(void) { return "Find and replace"; }
char **getdeps(void) { return deps; }
char *download(void) {
	cmd("git clone https://github.com/malloc-nbytes/far.git");
        return "far";
}
void build(void) {}
void install(void) {
	cmd("./install.sh");
}
void uninstall(void) {
	cmd("rm /usr/local/bin/far.py");
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
