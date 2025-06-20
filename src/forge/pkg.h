#ifndef PKG_H_INCLUDED
#define PKG_H_INCLUDED

#include <stddef.h>

#define FORGE_GLOBAL __attribute__((visibility("default")))

typedef struct {
        char *(*name)(void);
        char *(*ver)(void);
        char *(*desc)(void);
        char **(*deps)(void);
        char *(*download)(void);
        void (*build)(void);
        void (*install)(void);
        void (*uninstall)(void);
        int (*update)(void);
} pkg;

int forge_pkg_git_update(void);

#endif // PKG_H_INCLUDED
