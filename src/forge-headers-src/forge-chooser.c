/*
 * forge: Forge your system
 * Copyright (C) 2025  malloc-nbytes
 * Contact: zdhdev@yahoo.com

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License along
 * with this program; if not, see <https://www.gnu.org/licenses/>.
*/

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <unistd.h>
#include <termios.h>
#include <string.h>
#include <sys/ttydefaults.h>

#include "forge/chooser.h"
#include "forge/ctrl.h"
#include "forge/colors.h"
#include "forge/str.h"
#include "forge/array.h"
#include "forge/utils.h"

#define FALLBACK_WIN_WIDTH 10
#define DEFAULT_MSG "Select an option:"

typedef struct {
        const char **choices;
        size_t choices_n;
        size_t sel;
        size_t win_height;
        size_t scroll_offset;
        struct {
                int mode;          // 1 if in search mode, 0 otherwise
                forge_str buffer;  // Search query buffer
                forge_str last;    // Last search query
                size_t_array matches; // Array of matching choice indices
                size_t current;    // Current match index
        } search;
} forge_chooser_context;

static volatile sig_atomic_t g_resize_flag = 0;
static forge_chooser_context *g_ctx = NULL;

static void dump_choices(const forge_chooser_context *ctx);

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
        if (ctx->choices_n > ctx->win_height - 2) {
                if (ctx->sel < ctx->scroll_offset) {
                        ctx->scroll_offset = ctx->sel;
                } else if (ctx->sel >= ctx->scroll_offset + ctx->win_height - 2) {
                        ctx->scroll_offset = ctx->sel - (ctx->win_height - 2) + 1;
                }
        } else {
                ctx->scroll_offset = 0; // No scrolling needed if all choices fit
        }
}

static void
down(forge_chooser_context *ctx)
{
        if (ctx->sel < ctx->choices_n - 1) {
                ctx->sel++;
                if (ctx->choices_n > ctx->win_height - 2 &&
                    ctx->sel >= ctx->scroll_offset + ctx->win_height - 2) {
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
top(forge_chooser_context *ctx)
{
        ctx->sel = 0;
        ctx->scroll_offset = 0; // Reset scroll to top
}

static void
bottom(forge_chooser_context *ctx)
{
        ctx->sel = ctx->choices_n - 1;
        if (ctx->choices_n > ctx->win_height - 2) {
                ctx->scroll_offset = ctx->choices_n - (ctx->win_height - 2);
        } else {
                ctx->scroll_offset = 0; // No scrolling needed if all choices fit
        }
}

static void
controls(const forge_chooser_context *ctx)
{
        if (ctx->search.mode) {
                return;
        }
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
               BOLD GREEN INVERT "g:top" RESET
               " "
               BOLD GREEN INVERT "G:bottom" RESET
               " "
               BOLD GREEN INVERT "/:search" RESET
               " "
               BOLD GREEN INVERT "n:next" RESET
               " "
               BOLD GREEN INVERT "N:prev" RESET
               " "
               BOLD GREEN INVERT "Enter:select" RESET
               " "
               BOLD GREEN INVERT "Space:select" RESET);
        fflush(stdout);
}

static void
search_prompt(const forge_chooser_context *ctx)
{
        printf("\033[%zu;1H", ctx->win_height); // Move to last row
        printf("\033[K");                       // Clear the line
        const char *query = forge_str_to_cstr(&ctx->search.buffer);
        printf(BOLD YELLOW "/" RESET "%s", query ? query : "");
        fflush(stdout);
}

static void
next_match(forge_chooser_context *ctx)
{
        if (ctx->search.matches.len == 0 || ctx->search.last.len == 0) {
                return; // No matches or no search performed
        }
        if (ctx->search.current + 1 < ctx->search.matches.len) {
                ctx->sel = ctx->search.matches.data[++ctx->search.current];
                if (ctx->choices_n > ctx->win_height - 2 &&
                    ctx->sel >= ctx->scroll_offset + ctx->win_height - 2) {
                        ctx->scroll_offset = ctx->sel - (ctx->win_height - 2) + 1;
                }
        }
}

static void
prev_match(forge_chooser_context *ctx)
{
        if (ctx->search.matches.len == 0 || ctx->search.last.len == 0) {
                return; // No matches or no search performed
        }
        if (ctx->search.current > 0) {
                ctx->sel = ctx->search.matches.data[--ctx->search.current];
                if (ctx->sel < ctx->scroll_offset) {
                        ctx->scroll_offset = ctx->sel;
                }
        }
}

static void
search(forge_chooser_context *ctx)
{
        ctx->search.mode = 1;
        forge_str_clear(&ctx->search.buffer);

        while (1) {
                dump_choices(ctx);
                search_prompt(ctx);

                char ch;
                forge_ctrl_input_type ty = forge_ctrl_get_input(&ch);

                if (ty == USER_INPUT_TYPE_NORMAL) {
                        if (ch == '\n') {
                                // Clear previous matches
                                dyn_array_clear(ctx->search.matches);
                                ctx->search.current = 0;

                                // Save the current search query
                                forge_str_destroy(&ctx->search.last);
                                ctx->search.last = ctx->search.buffer;
                                ctx->search.buffer = forge_str_create();

                                // Search for the query in the choices
                                const char *pattern = forge_str_to_cstr(&ctx->search.last);
                                if (pattern && pattern[0] != '\0') {
                                        for (size_t i = 0; i < ctx->choices_n; ++i) {
                                                if (forge_utils_regex(pattern, ctx->choices[i])) {
                                                        dyn_array_append(ctx->search.matches, i);
                                                }
                                        }
                                }

                                // Jump to first match if it exists
                                if (ctx->search.matches.len > 0) {
                                        ctx->sel = ctx->search.matches.data[0];
                                        if (ctx->choices_n > ctx->win_height - 2) {
                                                if (ctx->sel >= ctx->scroll_offset + ctx->win_height - 2) {
                                                        ctx->scroll_offset = ctx->sel - (ctx->win_height - 2) + 1;
                                                } else if (ctx->sel < ctx->scroll_offset) {
                                                        ctx->scroll_offset = ctx->sel;
                                                }
                                        }
                                }
                                ctx->search.mode = 0;
                                return;
                        } else if (BACKSPACE(ch)) {
                                forge_str_pop(&ctx->search.buffer);
                        } else if (ch >= 32 && ch <= 126) {
                                forge_str_append(&ctx->search.buffer, ch);
                        }
                } else if (ty == USER_INPUT_TYPE_CTRL && ch == CTRL_G) {
                        ctx->search.mode = 0;
                        return;
                } else if (ty == USER_INPUT_TYPE_NORMAL && ESCSEQ(ch)) {
                        ctx->search.mode = 0;
                        return;
                }

                // Handle resize during search
                if (g_resize_flag) {
                        update_window_size(ctx);
                        g_resize_flag = 0;
                        forge_ctrl_clear_terminal();
                        forge_ctrl_cursor_to_first_line();
                        printf("%s\n", DEFAULT_MSG);
                        dump_choices(ctx);
                        search_prompt(ctx);
                }
        }
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

static forge_chooser_context
context_create(const char **choices,
               size_t       choices_n,
               size_t       cpos)
{
        return (forge_chooser_context) {
                .choices = choices,
                .choices_n = choices_n,
                .sel = cpos,
                .win_height = FALLBACK_WIN_WIDTH,
                .scroll_offset = 0,
                .search = {
                        .mode = 0,
                        .buffer = forge_str_create(),
                        .last = forge_str_create(),
                        .matches = dyn_array_empty(size_t_array),
                        .current = 0
                }
        };
}

int
forge_chooser(const char  *msg,
              const char **choices,
              size_t       choices_n,
              size_t       cpos)
{
        struct termios term;
        struct sigaction sa;

        forge_chooser_context ctx = context_create(choices, choices_n, cpos);

        dyn_array_init_type(ctx.search.matches);

        g_ctx = &ctx;

        if (!forge_ctrl_get_terminal_xy(NULL, &ctx.win_height)) {
                fprintf(stderr, "could not get terminal height\n");
                goto cleanup;
        }
        if (!forge_ctrl_sigaction(&sa, sigwinch_handler, SIGWINCH)) {
                fprintf(stderr, "could not set up sigwinch, resizing will not work\n");
                goto cleanup;
        }
        if (!forge_ctrl_enable_raw_terminal(STDIN_FILENO, &term)) {
                fprintf(stderr, "could not enable terminal to raw mode\n");
                goto cleanup;
        }

        // Adjust scroll_offset based on initial cpos only if necessary
        if (ctx.choices_n > ctx.win_height - 2 &&
            ctx.sel >= ctx.win_height - 2) {
                ctx.scroll_offset = ctx.sel - (ctx.win_height - 2) + 1;
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
                if (!ctx.search.mode) {
                        controls(&ctx);
                }

                char ch;
                forge_ctrl_input_type ty = forge_ctrl_get_input(&ch);
                if (ctx.search.mode) {
                        // Input handled by search() when in search mode
                        search(&ctx);
                        continue;
                }

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
                        } else if (ch == 'k') up(&ctx);
                        else if (ch == 'j') down(&ctx);
                        else if (ch == 'g') top(&ctx);
                        else if (ch == 'G') bottom(&ctx);
                        else if (ch == '/') search(&ctx);
                        else if (ch == 'n') next_match(&ctx);
                        else if (ch == 'N') prev_match(&ctx);
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

 cleanup:
        forge_str_destroy(&ctx.search.buffer);
        forge_str_destroy(&ctx.search.last);
        dyn_array_free(ctx.search.matches);
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

static void
display_yesno(const forge_chooser_context *ctx)
{
        forge_ctrl_cursor_to_col(1);
        forge_ctrl_clear_line();
        forge_ctrl_cursor_to_col(1);

        putchar('\t');
        for (size_t i = 0; i < ctx->choices_n; ++i) {
                if (i == ctx->sel) {
                        printf(YELLOW BOLD " ");
                }
                printf("%s", ctx->choices[i]);
                if (i == ctx->sel) {
                        printf(" " RESET);
                }
                putchar('\t');
        }

        fflush(stdout);
}

int
forge_chooser_yesno(const char *msg,
                    const char *custom,
                    int cpos)
{
        struct termios term;
        struct sigaction sa;

        const char *choices[] = {
                "No",
                "Yes",
                custom ? custom : NULL,
        };
        size_t choices_n = custom ? 3 : 2;

        forge_chooser_context ctx = context_create(choices, choices_n, cpos);
        g_ctx = &ctx;

        int res = -1;

        if (!forge_ctrl_get_terminal_xy(NULL, &ctx.win_height)) {
                fprintf(stderr, "could not get terminal height\n");
                goto cleanup;
        }
        if (!forge_ctrl_sigaction(&sa, sigwinch_handler, SIGWINCH)) {
                fprintf(stderr, "could not set up sigwinch, resizing will not work\n");
                goto cleanup;
        }
        if (!forge_ctrl_enable_raw_terminal(STDIN_FILENO, &term)) {
                fprintf(stderr, "could not enable terminal to raw mode\n");
                goto cleanup;
        }

        res = cpos;

        printf("%s\n", msg ? msg : DEFAULT_MSG);
        while (1) {
                ctx.sel = (size_t)res;

                if (g_resize_flag) {
                        update_window_size(&ctx);
                        g_resize_flag = 0;
                        forge_ctrl_clear_terminal();
                }

                display_yesno(&ctx);

                char ch;
                forge_ctrl_input_type ty = forge_ctrl_get_input(&ch);

                switch (ty) {
                case USER_INPUT_TYPE_ARROW: {
                        if (ch == LEFT_ARROW) {
                                if (res > 0) --res;
                        }
                        else if (ch == RIGHT_ARROW) {
                                if (res < (int)ctx.choices_n-1) ++res;
                        }
                } break;
                case USER_INPUT_TYPE_NORMAL: {
                        if (ENTER(ch)) {
                                goto cleanup;
                        } else if (ch == ' ') {
                                goto cleanup;
                        }
                } break;
                default: break;
                }
        }

 cleanup:
        if (!forge_ctrl_disable_raw_terminal(STDIN_FILENO, &term)) {
                fprintf(stderr, "could not disable terminal to raw mode\n");
                exit(1);
        }

        dyn_array_free(ctx.search.matches);
        putchar('\n');

        return res;
}
