#ifndef ARRAY_H_INCLUDED
#define ARRAY_H_INCLUDED

#include <stddef.h>

#include "dyn_array.h"

DYN_ARRAY_TYPE(char *, str_array);
DYN_ARRAY_TYPE(size_t, size_t_array);

#endif // ARRAY_H_INCLUDED
