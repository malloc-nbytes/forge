#ifndef CLAP_H
#define CLAP_H

#ifdef CLAP_IMPL

#include <stddef.h>

static struct {
        int argc;
        char **argv;
} __clap_config = {
        .argc = 0,
        .argv = NULL,
};

void clap_init(int argc, char **argv) {
        __clap_config.argc = argc;
        __clap_config.argv = argv;
}

typedef struct {
        // Points to the start of the argument
        // after any hyphens (max 2).
        char *start;
        // The number of hyphens (max 2).
        size_t hyphc;
        // Points to the character after the
        // first equals is encountered.
        char *eq;
} Clap_Arg;

static char *__clap_eat(void) {
        if (__clap_config.argc <= 0) {
                return NULL;
        }
        --__clap_config.argc;
        return *(__clap_config.argv++);
}

static int clap_next(Clap_Arg *clap_arg) {
        clap_arg->start = NULL;
        clap_arg->hyphc = 0;
        clap_arg->eq = NULL;

        char *arg = __clap_eat();
        if (!arg) return 0;

        if (arg[0] == '-' && arg[1] && arg[1] == '-') {
                clap_arg->hyphc = 2;
                arg += 2;
        } else if (arg[0] == '-') {
                clap_arg->hyphc = 1;
                ++arg;
        } else {
                clap_arg->hyphc = 0;
        }

        for (size_t i = 0; arg[i]; ++i) {
                if (arg[i] == '=') {
                        arg[i] = '\0';
                        clap_arg->eq = arg+i+1;
                        break;
                }
        }

        clap_arg->start = arg;

        return 1;
}

#endif // CLAP_IMPL

#endif // CLAP_H
