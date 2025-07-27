#ifndef FORGE_VIEWER_H_INCLUDED
#define FORGE_VIEWER_H_INCLUDED

#include <stddef.h>
#include <termios.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
        char **data;
        size_t rows;
        size_t cols;
        size_t win_width;
        size_t win_height;
        size_t height_offset;
        size_t width_offset;
        struct termios old_termios;
        int linenos;

        struct {
                int mode;
                char *buffer;
                size_t len;
                size_t cap;
                // Last search query
                char *last;
        } search;

        struct {
                // array of matching row indices
                size_t *matches;
                size_t count;
                size_t cap;
                size_t current;
        } match;
} forge_viewer;

/**
 * Parameter: data    -> the lines to put into the viewer
 * Parameter: data_n  -> the number of lines in `data`
 * Parameter: linenos -> should we show line numbers? (experimental)
 * Returns: a new forge_viewer
 * Description: Create a new forge_viewer of `data` (copied),
 *              `data_n` lines long. No need to free()
 *              `data`.
 */
forge_viewer *forge_viewer_alloc(char **data, size_t data_n, int linenos);

/**
 * Parameter: v -> the viewer
 * Description: free() all memory used by the viewer
 */
void forge_viewer_free(forge_viewer *v);

/**
 * Parameter: v -> the viewer
 * Description: Display all lines in the viewer.
 *              This opens a `less`-like viewer.
 */
void forge_viewer_display(forge_viewer *v);

#ifdef __cplusplus
}
#endif

#endif // FORGE_VIEWER_H_INCLUDED
