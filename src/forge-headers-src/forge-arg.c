#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "forge/arg.h"
#include "forge/mem.h"

static char *
arg_eat(int *argc, char ***argv)
{
        if (*argc <= 0) { return NULL; }
        --(*argc);
        char *cur = **argv;
        (*argv)++;
        return cur;
}

forge_arg *
forge_arg_alloc(int    argc,
                char **argv,
                int    skip_first)
{
        if (skip_first) { --argc, ++argv; }

        forge_arg *hd = NULL, *tl = NULL;

        while (argc > 0) {
                forge_arg *it = forge_mem_malloc(sizeof(forge_arg));
                it->s = NULL;
                it->h = 0;
                it->eq = NULL;
                it->n = NULL;

                char *arg = strdup(arg_eat(&argc, &argv));

                if (arg[0] == '-' && arg[1] && arg[1] == '-') {
                        it->h = 2;
                        arg += 2;
                } else if (arg[0] == '-') {
                        it->h = 1;
                        ++arg;
                } else {
                        it->h = 0;
                }

                for (size_t i = 0; arg[i]; ++i) {
                        if (arg[i] == '=') {
                                arg[i] = '\0';
                                it->eq = strdup(arg+i+1);
                                break;
                        }
                }

                it->s = strdup(arg);

                if (!hd && !tl) {
                        hd = tl = it;
                } else {
                        forge_arg *tmp = tl;
                        tl = it;
                        tmp->n = tl;
                }
        }

        return hd;
}

void
forge_arg_free(forge_arg *arg)
{
        while (arg) {
                free(arg->s);
                free(arg->eq);
                forge_arg *tmp = arg;
                arg = arg->n;
                free(tmp);
        }
}
