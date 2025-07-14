#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>

#include <forge/mem.h>

void *
forge_mem_malloc(size_t nbytes)
{
        void *p = malloc(nbytes);
        if (!p) {
                fprintf(stderr, "forge_mem_malloc: failed to allocate: %s\n",
                        strerror(errno));
                return NULL;
        }
        return p;
}
