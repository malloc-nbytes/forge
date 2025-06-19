#include <forge/forge.h>

char *deps[] = {NULL}; // Must be NULL terminated

char *getname(void) { return "gitcheck"; }
char *getver(void) { return "1.0.0"; }
char *getdesc(void) { return "Check for unstaged git changes in directories"; }
char **getdeps(void) { return deps; }
char *download(void) {
	cmd("git clone https://www.github.com/malloc-nbytes/gitcheck.git/");
        return "gitcheck";
}
void build(void) {
	cd("src");
	cmd("make");
}
void install(void) {
	cd("src");
	cmd("make install");
}
void uninstall(void) {
	cmd("rm /usr/local/bin/gitcheck");
}
int update(void) { return 0; }

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
