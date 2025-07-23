#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <termios.h>

#include "forge/chooser.h"
#include "forge/ctrl.h"
#include "forge/colors.h"

#define FALLBACK_WIN_WIDTH 10
#define DEFAULT_MSG "Select an option:"

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
        } else if (ctx->sel >= ctx->scroll_offset + ctx->win_height - 2) { // Account for msg and controls
                ctx->scroll_offset = ctx->sel - (ctx->win_height - 2) + 1;
        }
}

static void
down(forge_chooser_context *ctx)
{
        if (ctx->sel < ctx->choices_n - 1) {
                ctx->sel++;
                if (ctx->sel >= ctx->scroll_offset + ctx->win_height - 2) { // Account for msg and controls
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
controls(const forge_chooser_context *ctx)
{
        printf("\033[%zu;1H", ctx->win_height); // Move to last row
        printf(BOLD RED INVERT "q:quit" RESET
               " "
               BOLD RED INVERT "C-c:quit" RESET
               " "
               BOLD GREEN INVERT "j:down" RESET
               " "
               BOLD GREEN INVERT "k:up" RESET
               " "
               BOLD GREEN INVERT "↑:up" RESET
               " "
               BOLD GREEN INVERT "↓:down" RESET
               " "
               BOLD GREEN INVERT "C-n:down" RESET
               " "
               BOLD GREEN INVERT "C-p:up" RESET
               " "
               BOLD GREEN INVERT "Enter:select" RESET
               " "
               BOLD GREEN INVERT "Space:select" RESET);
        fflush(stdout);
}

static void
dump_choices(const forge_chooser_context *ctx)
{
        // Move to second line for choices (first line reserved for msg)
        printf("\033[2;1H");

        size_t display_count = ctx->choices_n - ctx->scroll_offset;
        if (display_count > ctx->win_height - 2) { // Account for msg and controls
                display_count = ctx->win_height - 2;
        }

        for (size_t i = 0; i < display_count; i++) {
                size_t idx = ctx->scroll_offset + i;
                if (idx == ctx->sel) {
                        printf(BOLD YELLOW "> %s\n" RESET, ctx->choices[idx]);
                } else {
                        printf("  %s\n", ctx->choices[idx]);
                }
        }

        // Clear any remaining lines up to controls
        for (size_t i = display_count; i < ctx->win_height - 2; i++) {
                printf("\033[K\n"); // Clear line and move to next
        }
}

static void
top(forge_chooser_context *ctx)
{
        ctx->sel = 0;
        ctx->scroll_offset = 0; // Reset scroll to top
}

static void
bottom(forge_chooser_context *ctx)
{
        ctx->sel = ctx->choices_n - 1;
        if (ctx->choices_n > ctx->win_height - 2) { // Account for msg and controls
                ctx->scroll_offset = ctx->choices_n - (ctx->win_height - 2);
        } else {
                ctx->scroll_offset = 0; // No scrolling needed if all choices fit
        }
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

                // Print message on first line
                forge_ctrl_cursor_to_first_line();
                printf("%s\n", msg ? msg : DEFAULT_MSG);

                dump_choices(&ctx);
                controls(&ctx);

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
                        else if (ch == 'k') up(&ctx);
                        else if (ch == 'j') down(&ctx);
                        else if (ch == 'g') top(&ctx);
                        else if (ch == 'G') bottom(&ctx);
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
