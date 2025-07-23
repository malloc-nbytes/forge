#include "context.h"

#include <forge/colors.h>
#include <forge/ctrl.h>
#include <forge/io.h>
#include <forge/str.h>
#include <forge/lexer.h>

#include <assert.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>

const char *g_c_kwds[] = FORGE_LEXER_C_KEYWORDS;
const char *g_py_kwds[] = FORGE_LEXER_PY_KEYWORDS;

static inline void
down(fviewer_context *ctx)
{
        if (ctx->viewer_sel_idx < ctx->fps->len-1) {
                ++ctx->viewer_sel_idx;
        }
}

static inline void
up(fviewer_context *ctx)
{
        if (ctx->viewer_sel_idx != 0) {
                --ctx->viewer_sel_idx;
        }
}

static void
draw_choices(const fviewer_context *ctx)
{
        for (size_t i = 0; i < ctx->fps->len; ++i) {
                if (i == ctx->viewer_sel_idx) {
                        printf(INVERT);
                }
                printf("%s\n", ctx->fps->data[i]);
                if (i == ctx->viewer_sel_idx) {
                        printf(RESET);
                }
        }
        fflush(stdout);
}

void
display_ctx(fviewer_context *ctx)
{
        if (ctx->fps->len <= 1) {
                forge_viewer_display(ctx->viewers.data[0]);
                return;
        }

        while (1) {
                forge_ctrl_clear_terminal();
                draw_choices(ctx);

                char ch;
                forge_ctrl_input_type ty = forge_ctrl_get_input(&ch);

                switch (ty) {
                case USER_INPUT_TYPE_CTRL: {
                        if      (ch == CTRL('n')) { down(ctx); }
                        else if (ch == CTRL('p')) { up(ctx); }
                } break;
                case USER_INPUT_TYPE_ALT: break;
                case USER_INPUT_TYPE_ARROW: {
                        if      (ch == UP_ARROW) { up(ctx); }
                        else if (ch == DOWN_ARROW) { down(ctx); }
                } break;
                case USER_INPUT_TYPE_SHIFT_ARROW: break;
                case USER_INPUT_TYPE_NORMAL: {
                        if (ENTER(ch)) {
                                forge_viewer_display(ctx->viewers.data[ctx->viewer_sel_idx]);
                        } else if (ch == 'j') {
                                down(ctx);
                        } else if (ch == 'k') {
                                up(ctx);
                        } else if (ch == 'q' || ch == CTRL('q')) {
                                goto done;
                        }
                } break;
                case USER_INPUT_TYPE_UNKNOWN: break;
                default: break;
                }
        }

 done:
        (void)0;
        forge_ctrl_clear_terminal();
}

fviewer_context
fviewer_context_create(const str_array *filepaths)
{
        fviewer_context ctx = (fviewer_context) {
                .fps     = filepaths,
                .viewers = dyn_array_empty(viewer_array),
                .viewer_sel_idx = 0,
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

                        // Color C file only (for now)
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
