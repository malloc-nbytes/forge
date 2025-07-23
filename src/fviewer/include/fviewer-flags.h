#ifndef FVIEWER_FLAGS_H_INCLUDED
#define FVIEWER_FLAGS_H_INCLUDED

#define FVIEWER_FLAG_1HY_HELP 'h'
#define FVIEWER_FLAG_1HY_LINES 'l'

#define FVIEWER_FLAG_2HY_HELP "help"
#define FVIEWER_FLAG_2HY_LINES "lines"

typedef enum {
        FVIEWER_FT_LINES = 1 << 0,
} fviewer_flag_type;

void usage(void);

#endif // FVIEWER_FLAGS_H_INCLUDED
