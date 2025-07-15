#ifndef FORGE_CTRL_H_INCLUDED
#define FORGE_CTRL_H_INCLUDED

#include <termios.h>

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
} forge_ctrl_input_type;

int forge_ctrl_enable_raw_terminal(int fd, struct termios *old_termios);
int forge_ctrl_disable_raw_terminal(int fd, struct termios *old_termios);
forge_ctrl_input_type forge_ctrl_get_input(char *c);

#endif // FORGE_CTRL_H_INCLUDED
