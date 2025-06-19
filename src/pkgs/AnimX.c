#include <forge/pkg.h>
#include <forge/cmd.h>

char *getname(void) { return "AnimX"; }
char *getver(void) { return "1.0"; }
char *getdesc(void) { return "Animated Wallpapers for X"; }
char *download(void) {
        cmd("git clone https://www.github.com/malloc-nbytes/AnimX.git/");
        return "AnimX";
}
void build(void) {
        cmd("autoreconf --install");
        cmd("./configure");
        cmd("make");
}
void install(void) {
        cmd("sudo make install");
}
void uninstall(void) {
        cmd("sudo make uninstall");
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
