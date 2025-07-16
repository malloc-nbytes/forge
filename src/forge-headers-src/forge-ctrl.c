#include <assert.h>
#include <stdio.h>
#include <unistd.h>

#include "forge/ctrl.h"

static char
get_char(void)
{
        char ch;
        int _ = read(STDIN_FILENO, &ch, 1);
        (void)_;
        return ch;
}

forge_ctrl_input_type
forge_ctrl_get_input(char *c)
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
                /* else if (*c == CTRL_N || *c == CTRL_P || *c == CTRL_G || */
                /*          *c == CTRL_D || *c == CTRL_U || *c == CTRL_V || */
                /*          *c == CTRL_W || *c == CTRL_O || *c == CTRL_L || */
                /*          *c == CTRL_F || *c == CTRL_B || *c == CTRL_A || */
                /*          *c == CTRL_E || *c == CTRL_S || *c == CTRL_Q) { */
                /*         return USER_INPUT_TYPE_CTRL; */
                /* } */
                else if (*c >= CTRL_A && *c <= CTRL_Z) {
                        return USER_INPUT_TYPE_CTRL;
                }
                else return USER_INPUT_TYPE_NORMAL;
        }
        return USER_INPUT_TYPE_UNKNOWN;
}


int
forge_ctrl_enable_raw_terminal(int             fd,
                               struct termios *old_termios)
{
        struct termios raw;

        // Get current terminal attributes
        if (tcgetattr(fd, old_termios) == -1) {
                perror("tcgetattr failed");
                fprintf(stderr, "Could not get terminal attributes\n");
                return 0;
        }

        // Copy current attributes
        raw = *old_termios;

        // Modify for raw mode: disable ECHO, ICANON, and IXON
        raw.c_lflag &= ~(ECHO | ICANON);
        raw.c_iflag &= ~IXON;

        // Apply new settings
        if (tcsetattr(fd, TCSANOW, &raw) == -1) {
                perror("tcsetattr failed");
                fprintf(stderr, "Could not set terminal to raw mode\n");
                return 0;
        }

        return 1;
}

int
forge_ctrl_disable_raw_terminal(int             fd,
                                struct termios *old_termios)
{
        return tcsetattr(fd, TCSANOW, old_termios) != -1;
}

void
forge_ctrl_clear_terminal(void)
{
        printf("\033[2J");
        printf("\033[H");
        fflush(stdout);
}

void
forge_ctrl_clear_line(void)
{
    // Clear the entire line
    printf("\033[2K");

    // Move cursor to the start of the line
    printf("\033[0G");

    fflush(stdout);
}

void
forge_ctrl_cursor_to_col(int n)
{
    if (n < 1) n = 1;

    // Move cursor to column n (1-based)
    printf("\033[%dG", n);
    fflush(stdout);
}
