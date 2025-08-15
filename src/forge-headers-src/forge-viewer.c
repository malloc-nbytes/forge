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

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <errno.h>

#include "forge/viewer.h"
#include "forge/ctrl.h"
#include "forge/colors.h"
#include "forge/rdln.h"

#include "utils.h"

// For signal handler (resizing window)
static forge_viewer *current_viewer = NULL;

// Indicate resize occurred
static volatile sig_atomic_t resize_flag = 0;

static void
handle_sigwinch(int sig)
{
        (void)sig;
        if (current_viewer != NULL) {
                struct winsize w;
                if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
                        current_viewer->win_width = w.ws_col - 1;
                        current_viewer->win_height = w.ws_row - 1;

                        // Adjust height_offset to prevent going past the end
                        if (current_viewer->rows > current_viewer->win_height &&
                            current_viewer->height_offset > current_viewer->rows - current_viewer->win_height) {
                                current_viewer->height_offset = current_viewer->rows - current_viewer->win_height;
                        }
                        // Adjust width_offset to prevent going past the end
                        if (current_viewer->cols > current_viewer->win_width &&
                            current_viewer->width_offset > current_viewer->cols - current_viewer->win_width) {
                                current_viewer->width_offset = current_viewer->cols - current_viewer->win_width;
                        }
                }
                resize_flag = 1;
        }
}

forge_viewer *
forge_viewer_alloc(char **data, size_t data_n, int linenos)
{
        forge_viewer *m = (forge_viewer*)malloc(sizeof(forge_viewer));

        // Set up SIGWINCH handler
        struct sigaction sa = {0};
        sa.sa_handler = handle_sigwinch;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        if (sigaction(SIGWINCH, &sa, NULL) == -1) {
                perror("sigaction failed");
                fprintf(stderr, "could not set up SIGWINCH handler\n");
        }

        // Get initial window size
        struct winsize w;
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
                m->win_width = w.ws_col - 1;
                m->win_height = w.ws_row - 1;
        } else {
                perror("ioctl failed");
                fprintf(stderr, "could not get size of terminal. Undefined behavior may occur\n");
        }

        // Set terminal to raw mode
        if (!forge_ctrl_enable_raw_terminal(STDIN_FILENO, &m->old_termios)) {
                fprintf(stderr, "Failed to set terminal to raw mode\n");
                free(m);
                return NULL;
        }

        // Initialize viewer data
        size_t col_max = 0;
        for (size_t i = 0; i < data_n; ++i) {
                size_t n = strlen(data[i]);
                if (n > col_max) { col_max = n; }
        }

        m->data = (char **)malloc(sizeof(char *) * data_n);
        for (size_t i = 0; i < data_n; ++i) {
                m->data[i] = (char *)malloc(col_max + 1);
                size_t n = strlen(data[i]);
                for (size_t j = 0; j < col_max; ++j) {
                        if (j >= n) {
                                m->data[i][j] = ' ';
                        } else {
                                m->data[i][j] = data[i][j];
                        }
                }
                m->data[i][col_max] = '\0';
        }

        m->rows = data_n;
        m->cols = col_max;
        m->height_offset = 0;
        m->width_offset = 0;

        // Initialize search fields
        m->search.mode = 0;
        m->search.cap = 256;
        m->search.buffer = (char *)malloc(m->search.cap);
        m->search.buffer[0] = '\0';
        m->search.len = 0;

        // Initialize match fields
        m->search.last = (char *)malloc(m->search.cap);
        m->search.last[0] = '\0';
        m->match.matches = (size_t *)malloc(sizeof(size_t) * data_n);
        m->match.count = 0;
        m->match.cap = data_n;
        m->match.current = 0;

        m->linenos = linenos;

        current_viewer = m;

        return m;
}

static inline void
controls(const forge_viewer *m)
{
        if (m->search.mode) {
                return;
        }
        printf("\033[%zu;1H", m->win_height);
        printf("[" BOLD YELLOW "q" RESET "]");
        printf("[" BOLD YELLOW "↓|j|C-n" RESET "]");
        printf("[" BOLD YELLOW "↑|k|C-p" RESET "]");
        printf("[" BOLD YELLOW "← |h|C-b" RESET "]");
        printf("[" BOLD YELLOW "→ |l|C-f" RESET "]");
        printf("[" BOLD YELLOW "0|C-a" RESET "]");
        printf("[" BOLD YELLOW "$|C-e" RESET "]");
        printf("[" BOLD YELLOW "g" RESET "]");
        printf("[" BOLD YELLOW "G" RESET "]");
        printf("[" BOLD YELLOW "^D" RESET "]");
        printf("[" BOLD YELLOW "^U" RESET "]");
        printf("[" BOLD YELLOW "/" RESET "]");
        printf("[" BOLD YELLOW "n" RESET "]");
        printf("[" BOLD YELLOW "N" RESET "]");
        fflush(stdout);
}

void
forge_viewer_dump(const forge_viewer *m)
{
        forge_ctrl_clear_terminal();
        if (m->rows == 0 || m->win_height <= 1) {
                // If no rows or window too small, just show controls or nothing
                if (m->win_height >= 1) {
                        controls(m);
                }
                return;
        }

        const size_t lineno_width = 5;

        // Adjust display width for content based on whether line numbers are shown
        size_t content_width = m->linenos && m->win_width > lineno_width ? m->win_width - lineno_width : m->win_width;

        // Display viewer data up to win_height - 1
        size_t display_height = m->win_height - 1;
        size_t end_row = m->height_offset + display_height;
        if (end_row > m->rows) {
                end_row = m->rows; // Cap at total rows
        }

        for (size_t i = m->height_offset; i < end_row; ++i) {
                if (m->linenos) {
                        printf(ITALIC BOLD CYAN "%4zu " RESET, i + 1);
                }
                for (size_t j = m->width_offset; j < m->cols && j < m->width_offset + content_width; ++j) {
                        putchar(m->data[i][j]);
                }
                putchar('\n');
        }

        // Move to last row and print controls
        controls(m);
}

void
forge_viewer_free(forge_viewer *m)
{
        for (size_t i = 0; i < m->rows; ++i) {
                free(m->data[i]);
        }
        free(m->data);
        free(m->search.buffer);
        free(m->search.last);
        free(m->match.matches);
        forge_ctrl_disable_raw_terminal(STDIN_FILENO, &m->old_termios);
        free(m);
        current_viewer = NULL;
}

static inline void
down(forge_viewer *m)
{
        if (m->win_height <= 1 || m->rows <= m->win_height - 1) {
                m->height_offset = 0; // No scrolling if viewer fits or window too small
                return;
        }
        if (m->height_offset < m->rows - (m->win_height - 1)) {
                ++m->height_offset;
        }
}

static inline void
up(forge_viewer *m)
{
        if (m->height_offset > 0) {
                --m->height_offset;
        }
}

static inline void
left(forge_viewer *m)
{
        if (m->width_offset > 0) {
                --m->width_offset;
        }
}

static inline void
right(forge_viewer *m)
{
        if (m->win_width <= 1 || m->cols <= m->win_width) {
                m->width_offset = 0; // No scrolling if viewer fits or window too small
                return;
        }
        if (m->width_offset < m->cols - m->win_width) {
                ++m->width_offset;
        }
}

static inline void
bol(forge_viewer *m)
{
        m->width_offset = 0;
}

static inline void
eol(forge_viewer *m)
{
        if (m->win_width <= 1 || m->cols <= m->win_width) {
                m->width_offset = 0; // No scrolling if viewer fits or window too small
                return;
        }
        m->width_offset = m->cols - m->win_width;
}

static inline void
top(forge_viewer *m)
{
        m->height_offset = 0;
}

static inline void
bottom(forge_viewer *m)
{
        if (m->win_height <= 1 || m->rows <= m->win_height - 1) {
                m->height_offset = 0;
        } else {
                m->height_offset = m->rows - (m->win_height - 1);
        }
}

static inline void
page_down(forge_viewer *m)
{
        if (m->win_height <= 1 || m->rows <= m->win_height - 1) {
                m->height_offset = 0; // No scrolling if viewer fits or window too small
                return;
        }
        m->height_offset += m->win_height - 1;
        if (m->height_offset > m->rows - (m->win_height - 1)) {
                m->height_offset = m->rows - (m->win_height - 1);
        }
}

static inline void
page_up(forge_viewer *m)
{
        if (m->win_height <= 1 || m->rows <= m->win_height - 1) {
                m->height_offset = 0; // No scrolling if viewer fits or window too small
                return;
        }
        if (m->height_offset <= m->win_height - 1) {
                m->height_offset = 0;
                return;
        }
        m->height_offset -= m->win_height - 1;
}

static inline void
search_prompt(const forge_viewer *m)
{
        printf("\033[%zu;1H", m->win_height); // Move to last row
        printf("\033[K");                     // Clear the line
        printf(BOLD YELLOW "/" RESET "%s", m->search.buffer);
        fflush(stdout);
}

static inline void
next_match(forge_viewer *m)
{
        if (m->match.count == 0 || m->search.last[0] == '\0') {
                return; // No matches or no search performed
        }
        if (m->match.current + 1 < m->match.count) {
                m->height_offset = m->match.matches[++m->match.current];
        }
}

static inline void
prev_match(forge_viewer *m)
{
        if (m->match.count == 0 || m->search.last[0] == '\0') {
                return; // No matches or no search performed
        }
        if (m->match.current > 0) {
                m->height_offset = m->match.matches[--m->match.current];
        }
}

static void
search(forge_viewer *m)
{
        m->search.mode = 1;
        m->search.buffer[0] = '\0';
        m->search.len = 0;

        while (1) {
                forge_viewer_dump(m);
                search_prompt(m);

                char ch;
                forge_ctrl_input_type ty = forge_ctrl_get_input(&ch);

                if (ty == USER_INPUT_TYPE_NORMAL) {
                        if (ch == '\n') {
                                // Clear previous matches
                                m->match.count = 0;
                                m->match.current = 0;

                                strcpy(m->search.last, m->search.buffer);

                                // Search for the query in the viewer and store all matches
                                for (size_t i = 0; i < m->rows; ++i) {
                                        if (strstr(m->data[i], m->search.buffer) != NULL) {
                                                if (m->match.count >= m->match.cap) {
                                                        m->match.cap *= 2;
                                                        m->match.matches = (size_t *)realloc(m->match.matches, sizeof(size_t) * m->match.cap);
                                                }
                                                m->match.matches[m->match.count++] = i;
                                        }
                                }

                                // Jump to first match if it exists
                                if (m->match.count > 0) {
                                        m->height_offset = m->match.matches[0];
                                        m->match.current = 0;
                                }
                                m->search.mode = 0;
                                return;
                        } else if (BACKSPACE(ch)) {
                                if (m->search.len > 0) {
                                        m->search.buffer[--m->search.len] = '\0';
                                }
                        } else if (ch >= 32 && ch <= 126) {
                                if (m->search.len + 1 >= m->search.cap) {
                                        // Resize buffers if needed
                                        m->search.cap *= 2;
                                        m->search.buffer = (char *)realloc(m->search.buffer, m->search.cap);
                                        m->search.last = (char *)realloc(m->search.last, m->search.cap);
                                }
                                m->search.buffer[m->search.len++] = ch;
                                m->search.buffer[m->search.len] = '\0';
                        }
                } else if (ty == USER_INPUT_TYPE_CTRL && ch == CTRL_G) {
                        m->search.mode = 0;
                        return;
                } else if (ty == USER_INPUT_TYPE_NORMAL && ESCSEQ(ch)) {
                        m->search.mode = 0;
                        return;
                }

                // Handle resize during search
                if (resize_flag) {
                        forge_viewer_dump(m);
                        search_prompt(m);
                        resize_flag = 0;
                }
        }
}

static void
lineno(forge_viewer *m)
{
        char *n = (char *)forge_rdln("goto line: ");
        if (!n) {
                // Empty or invalid input
                return;
        }

        char *endptr;
        errno = 0;
        long i = strtol(n, &endptr, 10);

        // Check if the input is a valid number
        if (endptr == n || *endptr != '\0') {
                // Input is empty or contains non-numeric characters
                free(n);
                return;
        }

        // Check for overflow or underflow
        if (errno == ERANGE || i <= 0 || i > (long)m->rows) {
                free(n);
                return;
        }

        // Valid line number
        m->height_offset = (size_t)(i - 1);

        if (m->rows > m->win_height - 1 && m->height_offset > m->rows - (m->win_height - 1)) {
                m->height_offset = m->rows - (m->win_height - 1);
        }

        free(n);
}

void
forge_viewer_display(forge_viewer *m)
{
        current_viewer = m;
        while (1) {
                forge_viewer_dump(m);
                resize_flag = 0;

                char ch;
                forge_ctrl_input_type ty = forge_ctrl_get_input(&ch);

                if (m->search.mode) {
                        // All input is handled by search() when in search mode
                        continue;
                }

                switch (ty) {
                case USER_INPUT_TYPE_NORMAL:
                        if      (ch == 'k') { up(m); }
                        else if (ch == 'j') { down(m); }
                        else if (ch == 'h') { left(m); }
                        else if (ch == 'l') { right(m); }
                        else if (ch == '0') { bol(m); }
                        else if (ch == '$') { eol(m); }
                        else if (ch == 'g') { top(m); }
                        else if (ch == 'G') { bottom(m); }
                        else if (ch == '/') { search(m); }
                        else if (ch == 'n') { next_match(m); }
                        else if (ch == 'N') { prev_match(m); }
                        else if (ch == 'q') { goto done; }
                        else if (ch == ':') { lineno(m); }
                        break;
                case USER_INPUT_TYPE_CTRL:
                        if      (ch == CTRL_N) { down(m); }
                        else if (ch == CTRL_P) { up(m); }
                        else if (ch == CTRL_D) { page_down(m); }
                        else if (ch == CTRL_U) { page_up(m); }
                        else if (ch == CTRL_A) { bol(m); }
                        else if (ch == CTRL_E) { eol(m); }
                        else if (ch == CTRL_F) { right(m); }
                        else if (ch == CTRL_B) { left(m); }
                        break;
                case USER_INPUT_TYPE_ARROW:
                        if      (ch == UP_ARROW)    { up(m); }
                        else if (ch == DOWN_ARROW)  { down(m); }
                        else if (ch == LEFT_ARROW)  { left(m); }
                        else if (ch == RIGHT_ARROW) { right(m); }
                        break;
                default:
                        break;
                }

                // Redraw if resize occurred (SIGWINCH set the flag)
                if (resize_flag) {
                        forge_viewer_dump(m); // Immediate redraw after resize
                        resize_flag = 0;
                }
        }
 done:
        forge_ctrl_clear_terminal();
}
