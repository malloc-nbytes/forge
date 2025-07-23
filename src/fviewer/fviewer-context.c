#include <fviewer-context.h>
#include <fviewer-gl.h>

#include <forge/colors.h>
#include <forge/ctrl.h>
#include <forge/io.h>
#include <forge/str.h>
#include <forge/lexer.h>
#include <forge/chooser.h>

#include <assert.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <errno.h>

const char *g_c_kwds[] = FORGE_LEXER_C_KEYWORDS;
const char *g_py_kwds[] = FORGE_LEXER_PY_KEYWORDS;

void
display_ctx(fviewer_context *ctx)
{
        if (ctx->fps.len <= 1) {
                forge_viewer_display(ctx->viewers.data[0]);
                return;
        }

        int cpos = 0;
        while (1) {
                int idx = forge_chooser("Choose a file",
                                        (const char **)ctx->fps.data,
                                        ctx->fps.len,
                                        cpos);
                if (idx == -1) break;

                forge_viewer_display(ctx->viewers.data[idx]);
                cpos = idx;
        }

        forge_ctrl_clear_terminal();
}

static void
collect_files(const char *path, str_array *files)
{
        struct stat st;
        if (stat(path, &st) != 0) {
                fprintf(stderr, "Cannot stat %s: %s\n", path, strerror(errno));
                return;
        }

        if (S_ISREG(st.st_mode)) {
                // Regular file
                char *path_copy = strdup(path);
                if (!path_copy) {
                        fprintf(stderr, "Memory allocation failed for path %s\n", path);
                        return;
                }
                dyn_array_append(*files, path_copy);
        } else if (S_ISDIR(st.st_mode)) {
                // Directory, recurse
                DIR *dir = opendir(path);
                if (!dir) {
                        fprintf(stderr, "Cannot open directory %s: %s\n", path, strerror(errno));
                        return;
                }

                struct dirent *entry;
                while ((entry = readdir(dir)) != NULL) {
                        // Skip . and ..
                        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
                                continue;
                        }

                        // Construct full path
                        size_t path_len = strlen(path) + strlen(entry->d_name) + 2; // +1 for '/' and null terminator
                        char *full_path = malloc(path_len);
                        if (!full_path) {
                                fprintf(stderr, "Memory allocation failed for path %s/%s\n", path, entry->d_name);
                                continue;
                        }
                        snprintf(full_path, path_len, "%s/%s", path, entry->d_name);

                        // Recurse
                        collect_files(full_path, files);
                        free(full_path);
                }
                closedir(dir);
        }
}

fviewer_context
fviewer_context_create(const str_array *filepaths)
{
        fviewer_context ctx = (fviewer_context) {
                .fps     = dyn_array_empty(str_array),
                .viewers = dyn_array_empty(viewer_array),
        };

        // Collect all files recursively
        for (size_t i = 0; i < filepaths->len; ++i) {
                collect_files(filepaths->data[i], (str_array *)&ctx.fps);
        }

        // Process collected files
        for (size_t i = 0; i < ctx.fps.len; ++i) {
                const char *path = ctx.fps.data[i];
                if (!forge_io_filepath_exists(path)) {
                        fprintf(stderr, "Filepath does not exist: %s\n", path);
                        continue;
                }

                char *actual = NULL;
                const char *ext = forge_io_file_ext(path);
                char *src = forge_io_read_file_to_cstr(path);

                if (!src) {
                        fprintf(stderr, "Failed to read file: %s\n", path);
                        continue;
                }

                if (ext && !strcmp(ext, "c")) {
                        actual = forge_colors_code_to_string(src, g_c_kwds);
                        free(src);
                } else if (ext && !strcmp(ext, "py")) {
                        actual = forge_colors_code_to_string(src, g_py_kwds);
                        free(src);
                } else {
                        actual = src;
                }

                size_t lines_n = 0;
                char **lines = forge_str_take_to_lines(actual, &lines_n);

                forge_viewer *v = forge_viewer_alloc(lines, lines_n);
                for (size_t j = 0; j < lines_n; ++j) free(lines[j]);
                dyn_array_append(ctx.viewers, v);
        }

        return ctx;
}
