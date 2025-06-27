#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "matrix.h"

matrix
matrix_create(const char **data,
              size_t       data_n)
{
        matrix m = {0};

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

        return m;
}

void
matrix_display(const matrix *m)
{
        for (size_t i = 0; i < m->rows; ++i) {
                for (size_t j = 0; j < m->cols; ++j) {
                        putchar(m->data[i][j]);
                }
                putchar('\n');
        }
}

/* void */
/* matrix_dump() */
