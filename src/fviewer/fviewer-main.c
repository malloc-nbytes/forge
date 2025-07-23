#include "fviewer-context.h"
#include "fviewer-flags.h"

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

struct {
        uint32_t flags;
} g_config = {
        .flags = 0x0000,
};

static void
handle_args(forge_arg *arghd)
{
        str_array filepaths = dyn_array_empty(str_array);

        forge_arg *arg = arghd;
        while (arg) {
                if (!arg->h) {
                        dyn_array_append(filepaths, strdup(arg->s));
                } else if (arg->h == 1) {
                        for (size_t i = 0; arg->s[i]; ++i) {
                                switch (arg->s[i]) {
                                case FVIEWER_FLAG_1HY_HELP: {
                                        usage();
                                } break;
                                case FVIEWER_FLAG_1HY_LINES: {
                                        g_config.flags |= FVIEWER_FT_LINES;
                                } break;
                                default: {
                                        fprintf(stderr, "unknown flag: %c\n", arg->s[i]);
                                        exit(1);
                                } break;
                                }
                        }
                } else { // --
                        if (!strcmp(arg->s, FVIEWER_FLAG_2HY_HELP)) {
                                usage();
                        } else if (!strcmp(arg->s, FVIEWER_FLAG_2HY_LINES)) {
                                g_config.flags |= FVIEWER_FT_LINES;
                        }
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
