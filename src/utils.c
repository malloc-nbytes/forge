#include "utils.h"

#include "forge/array.h"

#include <string.h>
#include <sys/stat.h>
#include <errno.h>

int
mkdir_p_wmode(const char *path, mode_t mode)
{
        if (!path || !*path) return -1;

        // Try to create the directory
        if (mkdir(path, mode) == 0 || errno == EEXIST) {
                return 0;
        }

        if (errno != ENOENT) {
                return -1;
        }

        // Find the last '/'
        char *parent = strdup(path);
        if (!parent) return -1;

        char *last_slash = strrchr(parent, '/');
        if (!last_slash || last_slash == parent) {
                free(parent);
                return -1;
        }

        *last_slash = '\0';

        // Recursively create parent directories
        int result = mkdir_p_wmode(parent, mode);
        free(parent);

        if (result != 0) return -1;

        // Try creating the directory again
        return mkdir(path, mode) == 0 || errno == EEXIST ? 0 : -1;
}
