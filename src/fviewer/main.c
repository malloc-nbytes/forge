#include <stdio.h>

#include <forge/viewer.h>
#include <forge/lexer.h>
#include <forge/arg.h>

void
usage(void)
{
        printf("Usage: fviewer [options] <files>\n");
}

void
handle_args(forge_arg *arghd)
{
        forge_arg *arg = arghd;
        while (arg) {
                arg = arg->n;
        }
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
