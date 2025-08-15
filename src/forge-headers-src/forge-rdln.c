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

static void
eol(rdln_context *ctx)
{
        ctx->cursor.pos = ctx->cursor.start + ctx->str.len;
}

static void
bol(rdln_context *ctx)
{
        ctx->cursor.pos = ctx->cursor.start;
}

static void
backspace(rdln_context *ctx)
{
        if (ctx->str.len > 0) {
                forge_str_rm_at(&ctx->str, ctx->cursor.pos - ctx->cursor.start - 1);
                --ctx->cursor.pos;
        }
}

static void
insert_char(rdln_context *ctx, char ch)
{
        forge_str_insert_at(&ctx->str, ch, ctx->cursor.pos - ctx->cursor.start);
        ++ctx->cursor.pos;
}

static void
del_until_eol(rdln_context *ctx)
{
        if (ctx->cursor.pos >= (int)(ctx->cursor.start + ctx->str.len)) {
                // Cursor is at or past the end
                return;
        }

        ctx->str.len = ctx->cursor.pos - ctx->cursor.start;
        memset(ctx->str.data + ctx->str.len, 0, ctx->str.cap - ctx->str.len);
}

static void
jump_forward_word(rdln_context *ctx)
{
        if (ctx->cursor.pos >= (int)(ctx->cursor.start + ctx->str.len)) {
                // Cursor is at or past the end; no movement
                return;
        }

        size_t idx = ctx->cursor.pos - ctx->cursor.start;

        // Skip current word (alphanumeric characters)
        while (idx < ctx->str.len && isalnum((unsigned char)ctx->str.data[idx])) {
                idx++;
        }

        // Skip following whitespace
        while (idx < ctx->str.len && !isalnum((unsigned char)ctx->str.data[idx])) {
                idx++;
        }

        ctx->cursor.pos = ctx->cursor.start + idx;
}

static void
jump_backward_word(rdln_context *ctx)
{
        if (ctx->cursor.pos <= (int)ctx->cursor.start) {
                // Cursor is at or before the start; no movement
                return;
        }

        size_t idx = ctx->cursor.pos - ctx->cursor.start;

        // Move back one if cursor is at the start of a word or after a word
        if (idx > 0) {
                idx--;
        }

        // Skip whitespace
        while (idx > 0 && !isalnum((unsigned char)ctx->str.data[idx])) {
                idx--;
        }

        // Skip word to find its start
        while (idx > 0 && isalnum((unsigned char)ctx->str.data[idx-1])) {
                idx--;
        }

        ctx->cursor.pos = ctx->cursor.start + idx;
}

static void
del_word_at_cursor(rdln_context *ctx)
{
        if (ctx->cursor.pos >= (int)(ctx->cursor.start + ctx->str.len)) {
                // Cursor is at or past the end; nothing to delete
                return;
        }

        size_t idx = ctx->cursor.pos - ctx->cursor.start;
        size_t start_idx = idx;

        // If in a word, move to its end
        while (idx < ctx->str.len && isalnum((unsigned char)ctx->str.data[idx])) {
                idx++;
        }

        // If in whitespace, move to next word's end
        if (idx == start_idx && idx < ctx->str.len) {
                // Skip whitespace
                while (idx < ctx->str.len && !isalnum((unsigned char)ctx->str.data[idx])) {
                        idx++;
                }
                // Skip word
                while (idx < ctx->str.len && isalnum((unsigned char)ctx->str.data[idx])) {
                        idx++;
                }
        }

        // Remove characters from start_idx to idx
        while (idx > start_idx) {
                forge_str_rm_at(&ctx->str, start_idx);
                idx--;
        }
}

static void
delete(rdln_context *ctx)
{
        right(ctx);
        backspace(ctx);
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

        forge_ctrl_clear_line();
        fflush(stdout);

        while (1) {
                drawln(&ctx);

                char ch = 0;
                forge_ctrl_input_type ty = forge_ctrl_get_input(&ch);

                switch (ty) {
                case USER_INPUT_TYPE_CTRL:
                        if (ch == CTRL_C) {
                                forge_ctrl_disable_raw_terminal(STDIN_FILENO, &term);
                                forge_str_destroy(&ctx.str);
                                return NULL;
                        }
                        else if (ch == CTRL_D) { delete(&ctx); }
                        else if (ch == CTRL_H) { backspace(&ctx); }
                        else if (ch == CTRL_B) { left(&ctx); }
                        else if (ch == CTRL_F) { right(&ctx); }
                        else if (ch == CTRL_E) { eol(&ctx); }
                        else if (ch == CTRL_A) { bol(&ctx); }
                        else if (ch == CTRL_K) { del_until_eol(&ctx); }
                        break;
                case USER_INPUT_TYPE_ALT:
                        if      (ch == 'f') { jump_forward_word(&ctx); }
                        else if (ch == 'b') { jump_backward_word(&ctx); }
                        else if (ch == 'd') { del_word_at_cursor(&ctx); }
                        break;
                case USER_INPUT_TYPE_ARROW:
                        if      (ch == LEFT_ARROW)  { left(&ctx); }
                        else if (ch == RIGHT_ARROW) { right(&ctx); }
                        break;
                case USER_INPUT_TYPE_SHIFT_ARROW: break;
                case USER_INPUT_TYPE_NORMAL:
                        if (BACKSPACE(ch) && ctx.cursor.pos > (int)ctx.cursor.start) {
                                backspace(&ctx);
                        }
                        else if (isprint((unsigned char)ch)) { insert_char(&ctx, ch); }
                        else if (ENTER(ch))                  { goto done; }
                        break;
                case USER_INPUT_TYPE_UNKNOWN: break;
                default: break;
                }
        }

 done:
        forge_str_append(&ctx.str, '\0');
        printf("\n");
        forge_ctrl_disable_raw_terminal(STDIN_FILENO, &term);
        return ctx.str.data;
}
