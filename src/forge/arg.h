#ifndef FORGE_ARG_H_INCLUDED
#define FORGE_ARG_H_INCLUDED

#include <stddef.h>

typedef struct forge_arg {
        // Points to the start of the argument
        // after any hyphens (max 2).
        char *s;

        // The number of hyphens (max 2).
        size_t h;

        // Points to the character after the
        // first equals is encountered.
        char *eq;

        // The next argument
        struct forge_arg *n;
} forge_arg;

/**
 * Parameter: argc       -> number of args
 * Parameter: argv       -> actual args
 * Parameter: skip_first -> skip the first arg
 * Returns: linked list of `forge_arg`s
 * Description: Create a list of args that you can
 *              query. The `skip_first` parameter is there
 *              if you do not want the program name to appear
 *              in the linked list. Also, you should always have
 *              a copy to the head of this list for `forge_arg_free()`.
 */
forge_arg *forge_arg_alloc(
        int    argc,
        char **argv,
        int    skip_first
);

/**
 * Parameter: arg -> the pointer to the start of the arg list
 * Description: Free all memory allocated for the linked
 *              list of forge_args. This function should
 *              take the head of a linked list.
 */
void forge_arg_free(forge_arg *arg);

#endif // FORGE_ARG_H_INCLUDED
