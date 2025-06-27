#ifndef MATRIX_H_INCLUDED
#define MATRIX_H_INCLUDED

typedef struct {
        char **data;
        size_t rows;
        size_t cols;
} matrix;

matrix matrix_create(const char **data, size_t data_n);
void matrix_display(const matrix *m);

#endif // MATRIX_H_INCLUDED
