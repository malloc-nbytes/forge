#ifndef VIEWER_H_INCLUDED
#define VIEWER_H_INCLUDED

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
} viewer;

viewer *viewer_alloc(char **data, size_t data_n);
void viewer_dump(const viewer *m);
void viewer_free(viewer *m);
void viewer_display(viewer *m);

#endif // VIEWER_H_INCLUDED
