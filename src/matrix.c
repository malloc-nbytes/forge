#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <signal.h>

#include "matrix.h"
#include "colors.h"
#include "utils.h"

#define CTRL_N 14
#define CTRL_D 4
#define CTRL_U 21
#define CTRL_L 12
#define CTRL_P 16
#define CTRL_G 7
#define CTRL_V 22
#define CTRL_W 23
#define CTRL_O 15
#define CTRL_F 6
#define CTRL_B 2
#define CTRL_A 1
#define CTRL_E 5
#define CTRL_S 19
#define CTRL_Q 17

#define UP_ARROW      'A'
#define DOWN_ARROW    'B'
#define RIGHT_ARROW   'C'
#define LEFT_ARROW    'D'

#define ENTER(ch)     (ch) == '\n'
#define BACKSPACE(ch) (ch) == 8 || (ch) == 127
#define ESCSEQ(ch)    (ch) == 27
#define CSI(ch)       (ch) == '['
#define TAB(ch)       (ch) == '\t'

typedef enum {
    USER_INPUT_TYPE_CTRL,
    USER_INPUT_TYPE_ALT,
    USER_INPUT_TYPE_ARROW,
    USER_INPUT_TYPE_SHIFT_ARROW,
    USER_INPUT_TYPE_NORMAL,
    USER_INPUT_TYPE_UNKNOWN,
} user_input_type;

static matrix *current_matrix = NULL;         // For signal handler (resizing window)
static volatile sig_atomic_t resize_flag = 0; // Indicate resize occurred

static void
handle_sigwinch(int sig)
{
        (void)sig; // Unused
        if (current_matrix != NULL) {
                struct winsize w;
                if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
                        current_matrix->win_width = w.ws_col - 1;
                        current_matrix->win_height = w.ws_row - 1;

                        // Adjust height_offset to prevent going past the end
                        if (current_matrix->rows > current_matrix->win_height &&
                            current_matrix->height_offset > current_matrix->rows - current_matrix->win_height) {
                                current_matrix->height_offset = current_matrix->rows - current_matrix->win_height;
                        }
                }
                resize_flag = 1;
        }
}

matrix *
matrix_alloc(char **data, size_t data_n)
{
        matrix *m = (matrix*)malloc(sizeof(matrix));

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
        tcgetattr(STDIN_FILENO, &m->old_termios);
        struct termios raw = m->old_termios;
        raw.c_lflag &= ~(ECHO | ICANON);
        raw.c_iflag &= ~IXON;
        tcsetattr(STDIN_FILENO, TCSANOW, &raw);

        // Initialize matrix data
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
                m->data[i][col_max] = 0;
        }

        m->rows = data_n;
        m->cols = col_max;
        m->height_offset = 0;

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

        current_matrix = m;
        return m;
}

static void
reset_scrn(void)
{
        printf("\033[2J");
        printf("\033[H");
        fflush(stdout);
}

static inline void
controls(const matrix *m)
{
        if (m->search.mode) {
                return;
        }
        printf("\033[%zu;1H", m->win_height);
        printf(BOLD RED INVERT "q:quit" RESET
               " "
               BOLD RED INVERT "C-c:quit" RESET
               " "
               BOLD GREEN INVERT "j:down" RESET
               " "
               BOLD GREEN INVERT "k:up" RESET
               " "
               BOLD GREEN INVERT "g:top" RESET
               " "
               BOLD GREEN INVERT "G:bottom" RESET
               " "
               BOLD GREEN INVERT "^D:pgdn" RESET
               " "
               BOLD GREEN INVERT "^U:pgup" RESET
               " "
               BOLD GREEN INVERT "^N:down" RESET
               " "
               BOLD GREEN INVERT "^P:up" RESET
               " "
               BOLD GREEN INVERT "↑:up" RESET
               " "
               BOLD GREEN INVERT "↓:down" RESET
               " "
               BOLD GREEN INVERT "/:search" RESET
               " "
               BOLD GREEN INVERT "n:next" RESET
               " "
               BOLD GREEN INVERT "N:prev" RESET);
        fflush(stdout);
}

void
matrix_dump(const matrix *m)
{
        reset_scrn();
        if (m->rows == 0 || m->win_height <= 1) {
                // If no rows or window too small, just show controls or nothing
                if (m->win_height >= 1) {
                        controls(m);
                }
                return;
        }

        // Display matrix data up to win_height - 1
        size_t display_height = m->win_height - 1;
        size_t end_row = m->height_offset + display_height;
        if (end_row > m->rows) {
                end_row = m->rows; // Cap at total rows
        }
        for (size_t i = m->height_offset; i < end_row; ++i) {
                for (size_t j = 0; j < m->cols && j < m->win_width; ++j) {
                        putchar(m->data[i][j]);
                }
                putchar('\n');
        }

        // Move to last row and print controls
        controls(m);
}

void
matrix_free(matrix *m)
{
        for (size_t i = 0; i < m->rows; ++i) {
                free(m->data[i]);
        }
        free(m->data);
        free(m->search.buffer);
        free(m->search.last);
        free(m->match.matches);
        tcsetattr(STDIN_FILENO, TCSANOW, &m->old_termios);
        free(m);
        current_matrix = NULL;
}

static char
get_char(void)
{
        char ch;
        int _ = read(STDIN_FILENO, &ch, 1);
        (void)_;
        return ch;
}

user_input_type
get_user_input(char *c)
{
        assert(c);
        while (1) {
                *c = get_char();
                if (ESCSEQ(*c)) {
                        int next0 = get_char();
                        if (CSI(next0)) {
                                int next1 = get_char();
                                if (next1 >= '0' && next1 <= '9') { // Modifier key detected
                                        int semicolon = get_char();
                                        if (semicolon == ';') {
                                                int modifier = get_char();
                                                int arrow_key = get_char();
                                                if (modifier == '2') { // Shift modifier
                                                        switch (arrow_key) {
                                                        case 'A': *c = UP_ARROW;    return USER_INPUT_TYPE_SHIFT_ARROW;
                                                        case 'B': *c = DOWN_ARROW;  return USER_INPUT_TYPE_SHIFT_ARROW;
                                                        case 'C': *c = RIGHT_ARROW; return USER_INPUT_TYPE_SHIFT_ARROW;
                                                        case 'D': *c = LEFT_ARROW;  return USER_INPUT_TYPE_SHIFT_ARROW;
                                                        default: return USER_INPUT_TYPE_UNKNOWN;
                                                        }
                                                }
                                        }
                                        return USER_INPUT_TYPE_UNKNOWN;
                                } else { // Regular arrow key
                                        switch (next1) {
                                        case DOWN_ARROW:
                                        case RIGHT_ARROW:
                                        case LEFT_ARROW:
                                        case UP_ARROW:
                                                *c = next1;
                                                return USER_INPUT_TYPE_ARROW;
                                        default:
                                                return USER_INPUT_TYPE_UNKNOWN;
                                        }
                                }
                        } else { // [ALT] key
                                *c = next0;
                                return USER_INPUT_TYPE_ALT;
                        }
                }
                else if (*c == CTRL_N || *c == CTRL_P || *c == CTRL_G ||
                         *c == CTRL_D || *c == CTRL_U || *c == CTRL_V ||
                         *c == CTRL_W || *c == CTRL_O || *c == CTRL_L ||
                         *c == CTRL_F || *c == CTRL_B || *c == CTRL_A ||
                         *c == CTRL_E || *c == CTRL_S || *c == CTRL_Q) {
                        return USER_INPUT_TYPE_CTRL;
                }
                else return USER_INPUT_TYPE_NORMAL;
        }
        return USER_INPUT_TYPE_UNKNOWN;
}

static inline void
down(matrix *m)
{
        if (m->win_height <= 1 || m->rows <= m->win_height - 1) {
                m->height_offset = 0; // No scrolling if matrix fits or window too small
                return;
        }
        if (m->height_offset < m->rows - (m->win_height - 1)) {
                ++m->height_offset;
        }
}

static inline void
up(matrix *m)
{
        if (m->height_offset > 0) {
                --m->height_offset;
        }
}

static inline void
top(matrix *m)
{
        m->height_offset = 0;
}

static inline void
bottom(matrix *m)
{
        if (m->win_height <= 1 || m->rows <= m->win_height - 1) {
                m->height_offset = 0;
        } else {
                m->height_offset = m->rows - (m->win_height - 1);
        }
}

static inline void
page_down(matrix *m)
{
        if (m->win_height <= 1 || m->rows <= m->win_height - 1) {
                m->height_offset = 0; // No scrolling if matrix fits or window too small
                return;
        }
        if (m->height_offset + (m->win_height - 1) >= m->rows) {
                m->height_offset = m->rows - (m->win_height - 1);
                return;
        }
        m->height_offset += m->win_height - 1;
}

static inline void
page_up(matrix *m)
{
        if (m->win_height <= 1 || m->rows <= m->win_height - 1) {
                m->height_offset = 0; // No scrolling if matrix fits or window too small
                return;
        }
        if (m->height_offset <= m->win_height - 1) {
                m->height_offset = 0;
                return;
        }
        m->height_offset -= m->win_height - 1;
}

static inline void
search_prompt(const matrix *m)
{
        printf("\033[%zu;1H", m->win_height); // Move to last row
        printf("\033[K");                     // Clear the line
        printf(BOLD YELLOW "/" RESET "%s", m->search.buffer);
        fflush(stdout);
}

static inline void
next_match(matrix *m)
{
        if (m->match.count == 0 || m->search.last[0] == '\0') {
                return; // No matches or no search performed
        }
        if (m->match.current + 1 < m->match.count) {
                m->height_offset = m->match.matches[++m->match.current];
        }
}

static inline void
prev_match(matrix *m)
{
        if (m->match.count == 0 || m->search.last[0] == '\0') {
                return; // No matches or no search performed
        }
        if (m->match.current > 0) {
                m->height_offset = m->match.matches[--m->match.current];
        }
}

static void
search(matrix *m)
{
        m->search.mode = 1;
        m->search.buffer[0] = '\0';
        m->search.len = 0;

        while (1) {
                matrix_dump(m);
                search_prompt(m);

                char ch;
                user_input_type ty = get_user_input(&ch);

                if (ty == USER_INPUT_TYPE_NORMAL) {
                        if (ch == '\n') {
                                // Clear previous matches
                                m->match.count = 0;
                                m->match.current = 0;

                                strcpy(m->search.last, m->search.buffer);

                                // Search for the query in the matrix and store all matches
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
                        matrix_dump(m);
                        search_prompt(m);
                        resize_flag = 0;
                }
        }
}

void
matrix_display(matrix *m)
{
        current_matrix = m;
        while (1) {
                matrix_dump(m);
                resize_flag = 0;

                char ch;
                user_input_type ty = get_user_input(&ch);

                if (m->search.mode) {
                        // All input is handled by search() when in search mode
                        continue;
                }

                switch (ty) {
                case USER_INPUT_TYPE_NORMAL:
                        if      (ch == 'k') { up(m); }
                        else if (ch == 'j') { down(m); }
                        else if (ch == 'g') { top(m); }
                        else if (ch == 'G') { bottom(m); }
                        else if (ch == '/') { search(m); }
                        else if (ch == 'n') { next_match(m); }
                        else if (ch == 'N') { prev_match(m); }
                        else if (ch == 'q') { goto done; }
                        break;
                case USER_INPUT_TYPE_CTRL:
                        if      (ch == CTRL_N) { down(m); }
                        else if (ch == CTRL_P) { up(m); }
                        else if (ch == CTRL_D) { page_down(m); }
                        else if (ch == CTRL_U) { page_up(m); }
                        break;
                case USER_INPUT_TYPE_ARROW:
                        if      (ch == UP_ARROW) { up(m); }
                        else if (ch == DOWN_ARROW) { down(m); }
                        break;
                default:
                        break;
                }

                // Redraw if resize occurred (SIGWINCH set the flag)
                if (resize_flag) {
                        matrix_dump(m); // Immediate redraw after resize
                        resize_flag = 0;
                }
        }
 done:
        reset_scrn();
}
