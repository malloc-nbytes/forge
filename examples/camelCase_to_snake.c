#include <forge/forge.h>

char *deps[] = {"far"}; // Must be NULL terminated

char *getname(void) { return "camelCase_to_snake"; }
char *getver(void) { return "1.0.0"; }
char *getdesc(void) { return "Kill those camels!"; }
char **getdeps(void) { return deps; }
char *download(void) {
        cmd("git clone https://github.com/malloc-nbytes/camelCase_to_snake.git");
        return "camelCase_to_snake";
}
void build(void) {
        cd("src");
        cmd("./build.sh");
}
void install(void) {
        cd("src");
        cmd("./install.sh");
}
void uninstall(void) {
        cmd("rm /usr/local/bin/ccts");
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
        .update = forge_pkg_git_update,
};
