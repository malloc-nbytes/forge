#include <forge/forge.h>

char *getname(void) { return "bless"; }
char *getver(void) { return "1.0.0"; }
char *getdesc(void) { return "Better `less`"; }
char *download(void) {
        cmd("git clone https://www.github.com/malloc-nbytes/bless.git/");
        return "bless";
}
void build(void) {
        cmd("mkdir build");
        cd("build");
        cmd("cmake -S .. -B .");
        cmd("make");
}
void install(void) {
        cd("build");
        cmd("sudo make install");
}
void uninstall(void) {
        cd("build");
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
