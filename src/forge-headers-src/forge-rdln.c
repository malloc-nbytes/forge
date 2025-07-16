#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>
#include <ctype.h>

#include "forge/rdln.h"
#include "forge/ctrl.h"
#include "forge/str.h"

typedef struct {
        const char *prompt;
        forge_str str;
        struct {
                int start;
                int pos;
        } cursor;
} rdln_context;

static void
drawln(const rdln_context *ctx) {
        forge_ctrl_clear_line();
        if (ctx->prompt) {
                printf("%s", ctx->prompt);
        }
        if (ctx->str.len > 0) {
                printf("%s", ctx->str.data);
        }
        forge_ctrl_cursor_to_col(ctx->cursor.pos+1);
        fflush(stdout);
}

static void
left(rdln_context *ctx)
{
        if (ctx->cursor.pos - ctx->cursor.start > 0) {
                --ctx->cursor.pos;
        }
}

static void
right(rdln_context *ctx)
{
        if (ctx->cursor.pos - ctx->cursor.start < (int)ctx->str.len) {
                ++ctx->cursor.pos;
        }
}

char *
forge_rdln(const char *prompt)
{
        struct termios term;
        const size_t start = prompt ? strlen(prompt) : 0;

        rdln_context ctx = (rdln_context) {
                .prompt = prompt,
                .str = forge_str_create(),
                .cursor = {
                        .start = start,
                        .pos = start,
                },
        };

        if (!forge_ctrl_enable_raw_terminal(STDIN_FILENO, &term)) {
                fprintf(stderr, "Failed to set terminal to raw mode\n");
                return NULL;
        }

        while (1) {
                drawln(&ctx);

                char ch = 0;
                forge_ctrl_input_type ty = forge_ctrl_get_input(&ch);

                switch (ty) {
                case USER_INPUT_TYPE_CTRL:
                        if (ch == CTRL_C || ch == CTRL_D) {
                                forge_ctrl_disable_raw_terminal(STDIN_FILENO, &term);
                                forge_str_destroy(&ctx.str);
                                return NULL;
                        } else if (ENTER(ch)) {
                                goto done;
                        }
                        break;
                case USER_INPUT_TYPE_ALT:
                        break;
                case USER_INPUT_TYPE_ARROW:
                        if (ch == LEFT_ARROW) {
                                left(&ctx);
                        } else if (ch == RIGHT_ARROW) {
                                right(&ctx);
                        }
                        break;
                case USER_INPUT_TYPE_SHIFT_ARROW:
                        break;
                case USER_INPUT_TYPE_NORMAL:
                        if (BACKSPACE(ch) && ctx.cursor.pos > (int)ctx.cursor.start) {
                                forge_str_rm_at(&ctx.str, ctx.cursor.pos - ctx.cursor.start - 1);
                                --ctx.cursor.pos;
                        } else if (isprint((unsigned char)ch)) {
                                forge_str_insert_at(&ctx.str, ch, ctx.cursor.pos - ctx.cursor.start);
                                ++ctx.cursor.pos;
                        } else if (ENTER(ch)) {
                                goto done;
                        }
                        break;
                case USER_INPUT_TYPE_UNKNOWN:
                        break;
                default:
                        break;
                }
        }

 done:
        printf("\n");
        forge_ctrl_disable_raw_terminal(STDIN_FILENO, &term);
        return forge_str_to_cstr(&ctx.str);
}
