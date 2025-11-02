#ifndef FORGE_PAIR_H_INCLUDED
#define FORGE_PAIR_H_INCLUDED

#define FORGE_PAIR_TYPE(type1, type2, pairname) \
        typedef struct { \
                type1 l; \
                type2 r; \
        } pairname; \
        \
        pairname pairname##_create(type1 l, type2 r); \
        type1 pairname##_left(pairname *p); \
        type2 pairname##_right(pairname *p); \
        \
        pairname \
        pairname##_create(type1 l, type2 r) \
        { \
                return (pairname) { \
                        .l = l, \
                        .r = r, \
                }; \
        } \
        type1 \
        pairname##_left(pairname *p) \
        { \
                return p->l; \
        } \
        type2 \
        pairname##_right(pairname *p) \
        { \
                return p->r; \
        } \


#endif // FORGE_PAIR_H_INCLUDED
