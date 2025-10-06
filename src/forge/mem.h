#ifndef FORGE_MEM_H_INCLUDED
#define FORGE_MEM_H_INCLUDED

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void *forge_mem_malloc(size_t nbytes);

#ifdef __cplusplus
}
#endif

#endif // FORGE_MEM_H_INCLUDED
