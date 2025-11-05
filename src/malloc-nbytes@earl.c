#include <string.h>

#include <forge/forge.h>

char *getname(void) { return "malloc-nbytes@earl"; }
char *getver(void)  { return "0.9.7"; }
char *getdesc(void) { return "A scripting language to replace BASH"; }
char *getweb(void)  { return "https://www.github.com/malloc-nbytes/earl.git/"; }

static void dump(const char *id, const char **mods) {
        printf("%s\n", id);
        if (!mods) return;
        for (size_t i = 0; mods[i]; ++i) printf("\t%s\n", mods[i]);
}

char *download(const char **mods) {
        dump("download()", mods);
        return git_clone("malloc-nbytes", "earl");
}

int build(const char **mods) {
        dump("build()", mods);

        cmd("mkdir build");
        cd("build");

        int portable = 0;
        for (int i = 0; mods[i]; ++i) {
                if (!strcmp(mods[i], "portable")) {
                        portable = 1;
                        break;
                }
        }

        if (portable) {
                CMD("cmake -S .. -B . -DPORTABLE=ON", return 0);
        } else {
                CMD("cmake -S .. -B .", return 0);
        }

        return 1;
}

int install(const char **mods) {
        dump("install()", mods);
        cd("build");
        return make("install");
}

const char *mods[] = {"portable", NULL};

int my_update(const char **mods) {
        dump("my_update()", mods);
        return 0;
}

int my_pull(const char **mods) {
        dump("my_pull()", mods);
        return 1;
}

FORGE_GLOBAL pkg package = {
        .name            = getname,
        .ver             = getver,
        .desc            = getdesc,
        .web             = getweb,
        .deps            = NULL,
        .download        = download,
        .build           = build,
        .install         = install,
        .update          = my_update,
        .get_changes     = my_pull,
        .mods            = mods,
};
