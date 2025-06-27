#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>

#include "matrix.h"

#define CTRL_N 14 // Scroll down
#define CTRL_D 4  // Page down
#define CTRL_U 21 // Page up
#define CTRL_L 12 // Refresh
#define CTRL_P 16 // Scroll up
#define CTRL_G 7  // Cancel
#define CTRL_V 22 // Scroll down
#define CTRL_W 23 // Save buffer
#define CTRL_O 15 // Open buffer
#define CTRL_F 6 // Scroll right
#define CTRL_B 2 // Scroll left
#define CTRL_A 1 // Jump to beginning of line
#define CTRL_E 5 // Jump to end of line
#define CTRL_S 19 // Search
#define CTRL_Q 17 // qbuf

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

matrix
matrix_create(char **data, size_t data_n)
{
        matrix m = {0};

        struct winsize w;
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0)
                m.win_width = w.ws_col-1, m.win_height = w.ws_row-1;
        else {
                perror("ioctl failed");
                fprintf(stderr, "could not get size of terminal. Undefined behavior may occur\n");
        }

        tcgetattr(STDIN_FILENO, &m.old_termios);
        struct termios raw = m.old_termios;
        raw.c_lflag &= ~(ECHO | ICANON);
        raw.c_iflag &= ~IXON;
        tcsetattr(STDIN_FILENO, TCSANOW, &raw);

        size_t col_max = 0;
        for (size_t i = 0; i < data_n; ++i) {
                size_t n = strlen(data[i]);
                if (n > col_max) { col_max = n; }
        }

        m.data = (char **)malloc(sizeof(char *) * data_n);

        for (size_t i = 0; i < data_n; ++i) {
                m.data[i] = (char *)malloc(col_max + 1);
                size_t n = strlen(data[i]);
                for (size_t j = 0; j < col_max; ++j) {
                        if (j >= n) {
                                m.data[i][j] = ' ';
                        } else {
                                m.data[i][j] = data[i][j];
                        }
                }
                m.data[i][col_max] = 0;
        }

        m.rows = data_n;
        m.cols = col_max;
        m.height_offset = 0;

        return m;
}

void
matrix_dump(const matrix *m)
{
        for (size_t i = m->height_offset;
             i < m->height_offset + m->win_height; ++i) {
                for (size_t j = 0; j < m->cols; ++j) {
                        putchar(m->data[i][j]);
                }
                putchar('\n');
        }
}

void
matrix_free(matrix *m)
{
        for (size_t i = 0; i < m->rows; ++i) {
                free(m->data[i]);
        }
        free(m->data);
        tcsetattr(STDIN_FILENO, TCSANOW, &m->old_termios);
}

static void
reset_scrn(void)
{
        printf("\033[2J");
        printf("\033[H");
        fflush(stdout);
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
        if (m->height_offset >= m->rows-m->win_height) {
                return;
        }
        ++m->height_offset;
}

static inline void
up(matrix *m)
{
        if (m->height_offset == 0) { return; }
        --m->height_offset;
}

static inline void
top(matrix *m)
{
        m->height_offset = 0;
}

static inline void
bottom(matrix *m)
{
        m->height_offset = m->rows - m->win_height;
}

static inline void
page_down(matrix *m)
{
        if (m->height_offset >= m->cols - m->win_height) {
                bottom(m);
                return;
        }
        m->height_offset += m->win_height;
}

static inline void
page_up(matrix *m)
{
        if (m->height_offset <= m->win_height) {
                top(m);
                return;
        }
        m->height_offset -= m->win_height;
}

void
matrix_display(matrix *m)
{
        while (1) {
                matrix_dump(m);
                char ch;
                user_input_type ty = get_user_input(&ch);
                switch (ty) {
                case USER_INPUT_TYPE_NORMAL: {
                        if      (ch == 'k') { up(m); }
                        else if (ch == 'j') { down(m); }
                        else if (ch == 'g') { top(m); }
                        else if (ch == 'G') { bottom(m); }
                        else if (ch == 'q') { goto done; }
                } break;
                case USER_INPUT_TYPE_CTRL: {
                        if      (ch == CTRL_N) { down(m); }
                        else if (ch == CTRL_P) { up(m); }
                        else if (ch == CTRL_D) { page_down(m); }
                        else if (ch == CTRL_U) { page_up(m); }
                } break;
                default: {} break;
                }
        }
 done:
        reset_scrn();
}
