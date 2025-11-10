#ifndef FORGE_PKG_H_INCLUDED
#define FORGE_PKG_H_INCLUDED

#include <stddef.h>
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

// Put this before `pkg package = { ... }` to
// make it visible to forge.
#define FORGE_GLOBAL __attribute__((visibility("default")))

typedef struct {
        char *(*name)(void);
        char *(*ver)(void);
        char *(*desc)(void);
        char *(*web)(void);
        char **(*deps)(void);
        char *(*download)(const char *const *);
        int (*build)(const char *const *);
        int (*install)(const char *const *);
        int (*uninstall)(const char *const *);
        int (*update)(const char *const *);
        int (*get_changes)(const char *const *);
        const char **mods;
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
int forge_pkg_git_pull(void);

/**
 * Description: Used in the .update part of the pkg struct.
 *              Use this if you want to notify that updates
 *              need manual checking.
 */
#define forge_pkg_update_manual_check NULL

/**
 * Description: Used in the .get_changes part of the pkg struct.
 *              Use this if you want to completely redownload
 *              the source code to get the new changes.
 */
#define forge_pkg_get_changes_redownload NULL

#ifdef __cplusplus
}
#endif

#endif // FORGE_PKG_H_INCLUDED
