#include <forge/forge.h>

char *deps[] = {NULL}; // Must be NULL terminated

char *getname(void) { return "gf"; }
char *getver(void) { return "1.0.0"; }
char *getdesc(void) { return "A GDB frontend for Linux"; }
char **getdeps(void) { return deps; }
char *download(void) {
	cmd("git clone https://github.com/nakst/gf.git");
        return "gf";
}
void build(void) {
	cmd("./build.sh");
}
void install(void) {
	cmd("cp ./gf2 /usr/local/bin/");
}
void uninstall(void) {
	cmd("rm /usr/local/bin/gf2");
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
