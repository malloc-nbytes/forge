#ifndef PKG_H_INCLUDED
#define PKG_H_INCLUDED

#include <stddef.h>

// Put this before `pkg package = { ... }` to
// make it visible to forge.
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
        void (*get_changes)(void);
} pkg;

/**
 * Returns: 1 if it should re-download the package,
 *          or 0 if it shouldn't.
 * Description: Performs the built-in way of doing
 *              an update if the package uses git.
 */
int forge_pkg_git_update(void);

/**
 * Description: Performs the built-in way of pulling
 *              changes if a package uses git.
 */
void forge_pkg_git_pull(void);

#endif // PKG_H_INCLUDED
