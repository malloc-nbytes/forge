#include <forge/forge.h>

char *deps[] = {NULL}; // Must be NULL terminated

char *getname(void) { return "raylib"; }
char *getver(void) { return "5.5.0"; }
char *getdesc(void) { return "A simple and easy-to-use library to enjoy videogames programming"; }
char **getdeps(void) { return deps; }
char *download(void) {
	cmd("git clone https://github.com/raysan5/raylib.git");
        return "raylib";
}
void build(void) {
	cmd("mkdir build");
	cd("build");
	cmd("cmake ..");
	cmd("make -j12");
}
void install(void) {
	cd("build");
	cmd("make install");
	cmd("ldconfig");
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
