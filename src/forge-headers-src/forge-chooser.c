#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <termios.h>

#include "forge/chooser.h"
#include "forge/ctrl.h"
#include "forge/colors.h"

#define FALLBACK_WIN_WIDTH 10

typedef struct {
        const char **choices;
        size_t choices_n;
        size_t sel;
        size_t win_height;
        size_t scroll_offset;
} forge_chooser_context;

static volatile sig_atomic_t g_resize_flag = 0;
static forge_chooser_context *g_ctx = NULL;

static void
sigwinch_handler(int sig)
{
        (void)sig;
        g_resize_flag = 1;
}

static void
update_window_size(forge_chooser_context *ctx)
{
        if (!forge_ctrl_get_terminal_xy(NULL, &ctx->win_height)) {
                ctx->win_height = FALLBACK_WIN_WIDTH;
        }

        // Adjust scroll_offset if selection is out of view
        if (ctx->sel < ctx->scroll_offset) {
                ctx->scroll_offset = ctx->sel;
        } else if (ctx->sel >= ctx->scroll_offset + ctx->win_height) {
                ctx->scroll_offset = ctx->sel - ctx->win_height + 1;
        }
}

static void
down(forge_chooser_context *ctx)
{
        if (ctx->sel < ctx->choices_n - 1) {
                ctx->sel++;
                if (ctx->sel >= ctx->scroll_offset + ctx->win_height) {
                        ctx->scroll_offset++;
                }
        }
}

static void
up(forge_chooser_context *ctx)
{
        if (ctx->sel > 0) {
                ctx->sel--;
                if (ctx->sel < ctx->scroll_offset) {
                        ctx->scroll_offset--;
                }
        }
}

static void
dump_choices(const forge_chooser_context *ctx)
{
        size_t display_count = ctx->choices_n - ctx->scroll_offset;
        if (display_count > ctx->win_height) {
                display_count = ctx->win_height;
        }

        for (size_t i = 0; i < display_count; i++) {
                size_t idx = ctx->scroll_offset + i;
                if (idx == ctx->sel) {
                        printf(BOLD YELLOW "> %s\n" RESET, ctx->choices[idx]);
                } else {
                        printf("  %s\n", ctx->choices[idx]);
                }
        }

        fflush(stdout);
}

int
forge_chooser(const char  *msg,
              const char **choices,
              size_t       choices_n,
              size_t       cpos)
{
        struct termios term;
        struct sigaction sa;

        forge_chooser_context ctx = (forge_chooser_context) {
                .choices = choices,
                .choices_n = choices_n,
                .sel = cpos,
                .win_height = FALLBACK_WIN_WIDTH,
                .scroll_offset = 0,
        };
        g_ctx = &ctx;

        if (!forge_ctrl_get_terminal_xy(NULL, &ctx.win_height)) {
                fprintf(stderr, "could not get terminal height\n");
                return -1;
        }
        if (!forge_ctrl_sigaction(&sa, sigwinch_handler, SIGWINCH)) {
                fprintf(stderr, "could not set up sigwinch, resizing will not work\n");
                return -1;
        }
        if (!forge_ctrl_enable_raw_terminal(STDIN_FILENO, &term)) {
                fprintf(stderr, "could not enable terminal to raw mode\n");
                return -1;
        }

        while (1) {
                if (g_resize_flag) {
                        update_window_size(&ctx);
                        g_resize_flag = 0;
                        forge_ctrl_clear_terminal();
                }

                forge_ctrl_clear_terminal();
                if (msg) {
                        printf("%s\n", msg);
                }
                dump_choices(&ctx);

                char ch;
                forge_ctrl_input_type ty = forge_ctrl_get_input(&ch);
                switch (ty) {
                case USER_INPUT_TYPE_CTRL: {
                        if      (ch == CTRL('n')) down(&ctx);
                        else if (ch == CTRL('p')) up(&ctx);
                        else if (ch == CTRL('c')) {
                                ctx.sel = -1;
                                goto done;
                        }
                } break;
                case USER_INPUT_TYPE_ALT: break;
                case USER_INPUT_TYPE_ARROW: {
                        if      (ch == UP_ARROW)   up(&ctx);
                        else if (ch == DOWN_ARROW) down(&ctx);
                } break;
                case USER_INPUT_TYPE_SHIFT_ARROW: break;
                case USER_INPUT_TYPE_NORMAL: {
                        if (ENTER(ch)) goto done;
                        else if (ch == 'q' || ch == CTRL('q')) {
                                ctx.sel = -1;
                                goto done;
                        } else if (ch == ' ') {
                                goto done;
                        }
                } break;
                case USER_INPUT_TYPE_UNKNOWN: break;
                default: break;
                }
        }

 done:
        forge_ctrl_clear_terminal();

        if (!forge_ctrl_disable_raw_terminal(STDIN_FILENO, &term)) {
                fprintf(stderr, "could not disable terminal to raw mode\n");
                exit(1);
        }

        return (int)ctx.sel;
}

int
forge_chooser_take(const char  *msg,
                   char       **choices,
                   size_t       choices_n,
                   size_t       cpos)
{
        int res = forge_chooser(msg, (const char **)choices, choices_n, cpos);
        for (size_t i = 0; i < choices_n; ++i) {
                free(choices[i]);
        }
        free(choices);
        return res;
}
