#ifndef FORGE_ERR_H_INCLUDED
#define FORGE_ERR_H_INCLUDED

#include <stdio.h>
#include <stdlib.h>

#include "forge/colors.h"

#define forge_err(msg)                                  \
        do {                                            \
                fprintf(stderr, msg "\n");              \
                exit(1);                                \
        } while (0)

#define forge_err_wargs(msg, ...)                                       \
        do {                                                            \
                fprintf(stderr, msg "\n", __VA_ARGS__);                 \
                exit(1);                                                \
        } while (0)

#define forge_todo(msg) \
        do {                                                            \
                fprintf(stderr, YELLOW BOLD "TODO: " RESET msg "\n");   \
                exit(1);                                                \
        } while (0)

#endif // FORGE_ERR_H_INCLUDED
