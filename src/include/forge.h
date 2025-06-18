#ifndef FORGE_H_INCLUDED
#define FORGE_H_INCLUDED

#include <stddef.h>

#define FORGE_C_MODULE_TEMPLATE \
        "#include <forge.h>\n" \
        "\n" \
        "char *deps[] = {NULL}; // Must be NULL terminated\n" \
        "\n" \
        "char *getname(void) { return \"mypackage\"; }\n" \
        "char *getver(void) { return \"1.0.0\"; }\n" \
        "char *getdesc(void) { return \"My Description\"; }\n" \
        "char **getdeps(void) { return deps; }\n" \
        "void build(void) {}\n" \
        "void install(void) {}\n" \
        "void uninstall(void) {}\n" \
        "\n" \
        "FORGE_GLOBAL pkg package = {\n" \
        "        .name = getname,\n" \
        "        .ver = getver,\n" \
        "        .desc = getdesc,\n" \
        "        .deps = NULL,\n" \
        "        .build = build,\n" \
        "        .install = install,\n" \
        "        .uninstall = uninstall,\n" \
        "};"

#define FORGE_GLOBAL __attribute__((visibility("default")))

typedef struct {
        char *(*name)(void);
        char *(*ver)(void);
        char *(*desc)(void);
        char **(*deps)(void);
        void (*build)(void);
        void (*install)(void);
        void (*uninstall)(void);
} pkg;

int cd(const char *fp);
int cmd(const char *cmd);

#endif // FORGE_H_INCLUDED
