#ifndef FORGE_CTRL_H_INCLUDED
#define FORGE_CTRL_H_INCLUDED

#include <termios.h>
#include <signal.h>

#ifdef __cplusplus
extern "C" {
#endif

extern volatile sig_atomic_t forge_ctrl_resize_flag;

// Different control keys
#define CTRL_A 1
#define CTRL_B 2
#define CTRL_C 3
#define CTRL_D 4
#define CTRL_E 5
#define CTRL_F 6
#define CTRL_G 7
#define CTRL_H 8
#define CTRL_I 9
#define CTRL_J 10
#define CTRL_K 11
#define CTRL_L 12
#define CTRL_M 13
#define CTRL_N 14
#define CTRL_O 15
#define CTRL_P 16
#define CTRL_Q 17
#define CTRL_R 18
#define CTRL_S 19
#define CTRL_T 20
#define CTRL_U 21
#define CTRL_V 22
#define CTRL_W 23
#define CTRL_X 24
#define CTRL_Y 25
#define CTRL_Z 26

// Arrows
#define UP_ARROW    'A'
#define DOWN_ARROW  'B'
#define RIGHT_ARROW 'C'
#define LEFT_ARROW  'D'

/**
 * Parameter: ch -> the character to compare
 * Returns: whether `ch` is a newline
 * Description: Check if `ch` is a newline.
 */
#define ENTER(ch)     ((ch) == '\n')

/**
 * Parameter: ch -> the character to compare
 * Returns: whether `ch` is a backspace
 * Description: Check if `ch` is a backspace.
 */
#define BACKSPACE(ch) ((ch) == 8 || (ch) == 127)

/**
 * Parameter: ch -> the character to compare
 * Returns: whether `ch` is a tab
 * Description: Check if `ch` is a tab.
 */
#define TAB(ch)       ((ch) == '\t')

/**
 * Parameter: ch -> the character to compare
 * Returns: whether `ch` is an escape sequence
 * Description: Check if `ch` is an escape sequence.
 */
#define ESCSEQ(ch)    ((ch) == 27)

/**
 * Parameter: ch -> the character to compare
 * Returns: whether `ch` is a control sequence
 * Description: Check if `ch` is a control sequence.
 */
#define CSI(ch)       ((ch) == '[')

#define CURSOR_LEFT(n)  printf("\033[%dD", n);
#define CURSOR_RIGHT(n) printf("\033[%dC", n);
#define CURSOR_UP(n)    printf("\033[%dA", n);
#define CURSOR_DOWN(n)  printf("\033[%dB", n);

// Different input types.
typedef enum {
    USER_INPUT_TYPE_CTRL,
    USER_INPUT_TYPE_ALT,
    USER_INPUT_TYPE_ARROW,
    USER_INPUT_TYPE_SHIFT_ARROW,
    USER_INPUT_TYPE_NORMAL,
    USER_INPUT_TYPE_UNKNOWN,
} forge_ctrl_input_type;

/**
 * Parameter: fd          -> the file descriptor
 * Parameter: old_termios -> the termios to copy bits from
 * Parameter: win_width   -> the result of getting the window width
 * Parameter: win_height  -> the result of getting the window height
 * Returns: 1 on success, 0 on failure
 * Description: Enable the terminal raw mode. This disables the bits:
 *                  ECHO
 *                  ICANON
 *                  IXON.
 *              Note: If working with stdin, `fd` should be STDIN_FILENO.
 *              Set win_width = NULL and win_height = NULL if you do
 *              not desire this information.
 */
int forge_ctrl_enable_raw_terminal(
        int             fd,
        struct termios *old_termios,
        size_t         *win_width,
        size_t         *win_height
);

/**
 * Parameter: fd          -> the file descriptor
 * Parameter: old_termios -> the termios to copy bits from
 * Returns: 1 on success, 0 on failure
 * Description: Disables the terminal raw mode. The old termios
 *              should have the same memory address as the termios
 *              passed to `forge_ctrl_enable_raw_terminal()`.
 */
int forge_ctrl_disable_raw_terminal(int fd, struct termios *old_termios);

/**
 * Parameter: c -> the character to store to
 * Returns: the type of input the user entered
 * Description: Will read input from the user. The type of that
 *              input will be the return type (see enum forge_ctrl_input_type)
 *              and the actual byte data will be inside of `c`.
 */
forge_ctrl_input_type forge_ctrl_get_input(char *c);

/**
 * Description: Clear the terminal.
 */
void forge_ctrl_clear_terminal(void);

/**
 * Description: Clear the current line.
 */
void forge_ctrl_clear_line(void);

/**
 * Parameter: n -> the column number
 * Description: Move the cursor to column `n`.
 */
void forge_ctrl_cursor_to_col(int n);

#ifdef __cplusplus
}
#endif

#endif // FORGE_CTRL_H_INCLUDED
