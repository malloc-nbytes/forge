#ifndef CONTEXT_H_INCLUDED
#define CONTEXT_H_INCLUDED

#include <forge/array.h>
#include <forge/viewer.h>

#include <stddef.h>

DYN_ARRAY_TYPE(forge_viewer *, viewer_array);

typedef struct {
        const str_array *fps;
        viewer_array viewers;
} fviewer_context;

fviewer_context fviewer_context_create(const str_array *filepaths);
void display_ctx(fviewer_context *ctx);

#endif // CONTEXT_H_INCLUDED
