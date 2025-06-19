#include <forge/forge.h>

char *getname(void) { return "ampire"; }
char *getver(void) { return "0.1.5"; }
char *getdesc(void) { return "A music player in the terminal"; }
char *download(void) {
        cmd("git clone https://www.github.com/malloc-nbytes/ampire.git/ --recursive");
        return "ampire";
}
void build(void) {
        cmd("mkdir build");
        cd("build");
        cmd("cmake -S .. -B .");
        cmd("make -j12");
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
