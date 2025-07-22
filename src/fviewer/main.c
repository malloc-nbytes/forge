#include <forge/viewer.h>
#include <forge/lexer.h>
#include <forge/arg.h>
#include <forge/io.h>
#include <forge/array.h>
#include <forge/ctrl.h>
#include <forge/colors.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

DYN_ARRAY_TYPE(forge_viewer *, viewer_array);

typedef struct {
        const str_array *fps;
        viewer_array viewers;
        size_t viewer_sel_idx;
} fviewer_context;

void
usage(void)
{
        printf("Usage: fviewer [options] <files>\n");
        exit(0);
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
                        char **lines = forge_io_read_file_to_lines(path);
                        size_t lines_n = 0;
                        for (; lines[lines_n]; ++lines_n);
                        forge_viewer *v = forge_viewer_alloc(lines, lines_n);
                        for (size_t j = 0; lines[j]; ++j) free(lines[j]);
                        free(lines);
                        dyn_array_append(ctx.viewers, v);
                }
        }
        return ctx;
}

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

void
handle_args(forge_arg *arghd)
{
        str_array filepaths = dyn_array_empty(str_array);

        forge_arg *arg = arghd;
        while (arg) {
                if (!arg->h) {
                        dyn_array_append(filepaths, strdup(arg->s));
                } else if (arg->h == 1) {
                        assert(0);
                } else { // --
                        assert(0);
                }
                arg = arg->n;
        }

        fviewer_context ctx = fviewer_context_create(&filepaths);

        display_ctx(&ctx);

        for (size_t i = 0; i < filepaths.len; ++i) {
                free(filepaths.data[i]);
                forge_viewer_free(ctx.viewers.data[i]);
        }
        dyn_array_free(filepaths);
}

int
main(int argc, char **argv)
{
        if (argc <= 1) {
                usage();
        }

        forge_arg *arghd = forge_arg_alloc(argc, argv, 1);
        handle_args(arghd);
        forge_arg_free(arghd);

        return 0;
}
