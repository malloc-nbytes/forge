#ifndef MATRIX_H_INCLUDED
#define MATRIX_H_INCLUDED

#include <termios.h>

typedef struct {
        char **data;
        size_t rows;
        size_t cols;
        size_t win_width;
        size_t win_height;
        struct termios old_termios;
} matrix;

matrix matrix_create(const char **data, size_t data_n);
void matrix_display(const matrix *m);
void matrix_free(matrix *m);

#endif // MATRIX_H_INCLUDED
