#ifndef CPM_H_INCLUDED
#define CPM_H_INCLUDED

#include <stddef.h>

#define CPM_GLOBAL __attribute__((visibility("default")))

typedef struct {
        char *(*name)(void);
        char *(*ver)(void);
        char *(*desc)(void);
        char **(*deps)(void);
        int installed;
} pkg;

#endif // CPM_H_INCLUDED
