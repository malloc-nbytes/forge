#ifndef MATRIX_H_INCLUDED
#define MATRIX_H_INCLUDED

#include <termios.h>

typedef struct {
        char **data;
        size_t rows;
        size_t cols;
        size_t win_width;
        size_t win_height;
        size_t height_offset;
        struct termios old_termios;

        struct {
                int mode;
                char *buffer;
                size_t len;
                size_t cap;
                char *last; // Last search query
        } search;

        struct {
                size_t *matches; // array of matching row indices
                size_t count;
                size_t cap;
                size_t current;
        } match;
} matrix;

matrix *matrix_alloc(char **data, size_t data_n);
void matrix_dump(const matrix *m);
void matrix_free(matrix *m);
void matrix_display(matrix *m);

#endif // MATRIX_H_INCLUDED
