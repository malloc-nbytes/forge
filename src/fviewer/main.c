#include "context.h"

#include <forge/viewer.h>
#include <forge/lexer.h>
#include <forge/arg.h>
#include <forge/io.h>
#include <forge/array.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

void
usage(void)
{
        printf("Usage: fviewer [options] <files>\n");
        exit(0);
}

void
handle_args(forge_arg *arghd)
{
        str_array filepaths = dyn_array_empty(str_array);

        forge_arg *arg = arghd;
        while (arg) {
                if (!arg->h) {
                        dyn_array_append(filepaths, strdup(arg->s));
                } else if (arg->h == 1) {
                        assert(0);
                } else { // --
                        assert(0);
                }
                arg = arg->n;
        }

        fviewer_context ctx = fviewer_context_create(&filepaths);

        display_ctx(&ctx);

        for (size_t i = 0; i < filepaths.len; ++i) {
                free(filepaths.data[i]);
                forge_viewer_free(ctx.viewers.data[i]);
        }
        dyn_array_free(filepaths);
}

int
main(int argc, char **argv)
{
        if (argc <= 1) {
                usage();
        }

        forge_arg *arghd = forge_arg_alloc(argc, argv, 1);
        handle_args(arghd);
        forge_arg_free(arghd);

        return 0;
}
