#include "context.h"

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

const char *g_c_kwds[] = FORGE_LEXER_C_KEYWORDS;
const char *g_py_kwds[] = FORGE_LEXER_PY_KEYWORDS;

void
display_ctx(fviewer_context *ctx)
{
        if (ctx->fps->len <= 1) {
                forge_viewer_display(ctx->viewers.data[0]);
                return;
        }

        int cpos = 0;
        while (1) {
                int idx = forge_chooser((const char **)ctx->fps->data,
                                        ctx->fps->len,
                                        cpos);
                if (idx == -1) break;

                forge_viewer_display(ctx->viewers.data[idx]);
                cpos = idx;
        }

        forge_ctrl_clear_terminal();
}

fviewer_context
fviewer_context_create(const str_array *filepaths)
{
        fviewer_context ctx = (fviewer_context) {
                .fps     = filepaths,
                .viewers = dyn_array_empty(viewer_array),
        };

        for (size_t i = 0; i < filepaths->len; ++i) {
                const char *path = filepaths->data[i];
                if (!forge_io_filepath_exists(path)) {
                        assert(0 && "filepath does not exist");
                }
                if (forge_io_is_dir(path)) {
                        assert(0 && "directories unimplemented");
                } else {
                        char *actual = NULL;
                        const char *ext = forge_io_file_ext(path);
                        char *src = forge_io_read_file_to_cstr(path);

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
                        free(lines);
                        dyn_array_append(ctx.viewers, v);
                }
        }
        return ctx;
}
