#ifndef FORGE_SET_H_INCLUDED
#define FORGE_SET_H_INCLUDED

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>

#define FORGE_SET_DEFAULT_CAPACITY 256

#define forge_set_type(type, setname) \
        typedef unsigned (*forge_##setname##_hash_sig)(type *); \
        typedef int      (*forge_##setname##_cmp_sig)(type *, type *); \
        typedef void     (*forge_##setname##_vfree_sig)(type *); \
        \
        typedef struct __##setname##_node { \
                type v; \
                struct __##setname##_node *n; \
        } __##setname##_node; \
        \
        typedef struct { \
                struct { \
                        __##setname##_node **data; \
                        size_t len; \
                        size_t cap; \
                        size_t sz; \
                } tbl; \
                forge_##setname##_hash_sig hash; \
                forge_##setname##_cmp_sig cmp; /*returns 0 on equal*/   \
                forge_##setname##_vfree_sig vfree; \
        } setname; \
        \
        setname setname##_create(forge_##setname##_hash_sig hash, forge_##setname##_cmp_sig cmp, forge_##setname##_vfree_sig vfree); \
        void    setname##_insert(setname *s, type v); \
        void    setname##_remove(setname *s, type v); \
        int     setname##_contains(const setname *s, type v); \
        void    setname##_destroy(setname *s); \
        void    setname##_print(const setname *s, void (*show)(type *t)); \
        \
        setname \
        setname##_create(forge_##setname##_hash_sig hash, \
                         forge_##setname##_cmp_sig cmp, \
                         forge_##setname##_vfree_sig vfree) \
        { \
                assert(hash); \
                assert(cmp); \
                __##setname##_node **data \
                        = (__##setname##_node **)calloc(FORGE_SET_DEFAULT_CAPACITY, sizeof(__##setname##_node *)); \
                return (setname) { \
                        .tbl = { \
                                .data = data, \
                                .len  = 0, \
                                .cap  = FORGE_SET_DEFAULT_CAPACITY, \
                                .sz   = 0, \
                        }, \
                        .hash = hash, \
                        .cmp = cmp, \
                        .vfree = vfree, \
                }; \
        } \
        void \
        setname##_insert(setname *s, type v) \
        { \
                unsigned idx = s->hash(&v) % s->tbl.cap; \
                __##setname##_node *it = s->tbl.data[idx]; \
                __##setname##_node *prev = NULL; \
                while (it) { \
                        if (s->cmp(&it->v, &v) == 0) return; \
                        prev = it; \
                        it = it->n; \
                } \
                __##setname##_node *n = (__##setname##_node *)malloc(sizeof(__##setname##_node)); \
                n->v = v; \
                n->n = NULL; \
                if (prev) { \
                        prev->n = n; \
                } else { \
                        s->tbl.data[idx] = n; \
                        ++s->tbl.len; \
                } \
                ++s->tbl.sz; \
        } \
        \
        void \
        setname##_remove(setname *s, type v) \
        { \
                unsigned idx = s->hash(&v) % s->tbl.cap; \
                __##setname##_node *it = s->tbl.data[idx]; \
                __##setname##_node *prev = NULL; \
                while (it) { \
                        if (s->cmp(&it->v, &v) == 0) { \
                                if (prev) { \
                                        prev->n = it->n; \
                                        /* TODO: free 'it' */ \
                                } else { \
                                        prev = it->n; \
                                        s->tbl.data[idx] = prev; \
                                        /* TODO: free 'it' */ \
                                } \
                                --s->tbl.sz; \
                        } \
                        prev = it; \
                        it = it->n; \
                } \
        } \
        \
        int \
        setname##_contains(const setname *s, type v) \
        { \
                unsigned idx = s->hash(&v) % s->tbl.cap; \
                __##setname##_node *it = s->tbl.data[idx]; \
                while (it) { \
                        if (s->cmp(&it->v, &v) == 0) return 1; \
                        it = it->n; \
                } \
                return 0; \
        } \
        \
        void \
        setname##_destroy(setname *s) \
        { \
                for (size_t i = 0; i < s->tbl.cap; ++i) { \
                        __##setname##_node *it = s->tbl.data[i]; \
                        while (it) { \
                                if (s->vfree) s->vfree(&it->v); \
                                __##setname##_node *tmp = it; \
                                it = it->n; \
                                free(tmp); \
                        } \
                } \
        } \
        \
        void \
        setname##_print(const setname *s, \
                        void (*show)(type *t)) \
        { \
                for (size_t i = 0; i < s->tbl.cap; ++i) { \
                        __##setname##_node *it = s->tbl.data[i]; \
                        while (it) { \
                                show(&it->v); \
                                it = it->n; \
                        } \
                } \
        } \

#endif // FORGE_SET_H_INCLUDED
