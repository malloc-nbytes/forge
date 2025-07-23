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
forge_chooser(const char **choices,
              size_t       choices_n)
{
        struct termios term;

        forge_chooser_context ctx = (forge_chooser_context) {
                .choices = choices,
                .choices_n = choices_n,
                .sel = 0,
                .win_height = FALLBACK_WIN_WIDTH,
                .scroll_offset = 0,
        };

        if (!forge_ctrl_enable_raw_terminal(STDIN_FILENO, &term, NULL, &ctx.win_height)) {
                fprintf(stderr, "could not enable terminal to raw mode\n");
                exit(1);
        }

        while (1) {
                forge_ctrl_clear_terminal();
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
forge_chooser_take(char       **choices,
                   size_t       choices_n)
{
        int res = forge_chooser((const char **)choices, choices_n);
        for (size_t i = 0; i < choices_n; ++i) {
                free(choices[i]);
        }
        free(choices);
        return res;
}
